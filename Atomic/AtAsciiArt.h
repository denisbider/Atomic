#pragma once

#include "AtAuto.h"
#include "AtException.h"
#include "AtVec.h"


namespace At
{
	namespace AsciiArt
	{
		
		struct DecodeErr : public StrErr { DecodeErr(Seq s) : StrErr(s) {} };


		struct ColorVal
		{
			uint m_alpha {};
			uint m_red   {};
			uint m_green {};
			uint m_blue  {};
			uint m_value {};

			void Read(Seq& reader);
			void Apply(ColorVal& result) const { result = *this; }
			void Interpolate(ColorVal const& a, ColorVal const& b, ColorVal const& c, ColorVal const& d);
			void MakeValue();

		private:
			uint ReadValue(Seq& reader, char const* name);
			uint AverageOf(uint a, uint b, uint c, uint d) { return (a + b + c + d) / 4; }
			uint WeightedAverageOf(uint aAlpha, uint a, uint bAlpha, uint b, uint cAlpha, uint c, uint dAlpha, uint d);
		};


		struct Gradient
		{
			double   m_angle;
			double   m_distanceMin;
			double   m_distanceMax;
			ColorVal m_colorMin;
			ColorVal m_colorMax;

			void Read(Seq& reader);
			void Apply(ColorVal& result, uint rowIndex, uint colIndex) const;

		private:
			enum { MaxDistanceFactor = 0x10000 };

			double m_lineSin;
			double m_lineCos;

			double PointDistance(double rowIndex, double colIndex) const;
			uint ValueWeightedByDistanceFactor(uint distanceFactor, uint minDistVal, uint maxDistVal) const;
		};


		enum class BrushType { Color, Gradient };

		struct Brush
		{
			BrushType          m_type {};
			AutoFree<ColorVal> m_color;
			AutoFree<Gradient> m_gradient;

			void Read(Seq& reader);
			void Apply(ColorVal& result, uint rowIndex, uint colIndex) const;
		};


		struct Icon
		{
			enum { MaxDimension = 256 };

			uint               m_width  {};
			uint               m_height {};
			Vec<Brush>         m_brushes { 126 - 32 };
			Vec<Vec<ColorVal>> m_rows;
			bool               m_encode {};

			void Read(Seq& reader, Icon const* prevIcon);

		protected:
			Brush&       GetBrush(uint pixelChar)       { return m_brushes[pixelChar - 32]; }
			Brush const& GetBrush(uint pixelChar) const { return m_brushes[pixelChar - 32]; }

		private:
			void MakeHalfSize(Icon const& orig);
		};


		struct IconAndBmp : public Icon
		{
			Str    m_bmp;
			uint32 m_fileOffset;

			void Encode();
		};


		// Format:
		//
		//   art := image *(*newline image)
		//
		//   image := image-def *add-half-size
		//
		//   image-def :=
		//     width "x" height [" " no-encode] newline
		//     1*(brush-definition newline)
		//     1*pixel-row													-- assumed space-padded if fewer rows than height
		//
		//   pixel-row := "`" 1*pixel-char-pair newline						-- assumed space-padded on right if fewer pixels than width
		//
		//   add-half-size := "add-half-size" [" " no-encode] newline		-- add after a higher resolution image to insert a half-size interpolated copy
		//
		//   width, height := a decimal integer
		//   no-encode := "no-encode"
		//   brush-definition := ":" pixel-char " " (color-definition | gradient-definition)
		//   color-definition := "color " color-value
		//   gradient-definition := "gradient " angle-degrees " " distance-min " " distance-max " " color-min " " color-max
		//   angle-degrees, distance-min, distance-max := decimal digits with optional fraction
		//   color-value, color-min, color-max := alpha red green blue
		//   alpha, red, green, blue := hex digit
		//   pixel-char-pair := pixel-char pixel-char						-- same pixel-char twice - so the ASCII art looks kinda proportional
		//   pixel-char := any printable ASCII character (32-126) including space

		Str EncodeIco(Seq art);

	}
}
