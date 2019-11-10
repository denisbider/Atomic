#include "AtIncludes.h"
#include "AtAsciiArt.h"


namespace At
{
	namespace AsciiArt
	{

		// ColorVal

		uint ColorVal::ReadValue(Seq& reader, char const* name)
		{
			Seq digit = reader.ReadBytes(1);
			uint value = digit.ReadNrUInt32(16);
			if (value == UINT_MAX)
				throw DecodeErr(Str(__FUNCTION__ ": Invalid value: ").Add(name));

			// 0x00 becomes 0x00, 0x01 becomes 0x11, ... 0x0f becomes 0xff
			return 17 * value;
		}


		void ColorVal::Read(Seq& reader)
		{
			m_alpha = ReadValue(reader, "alpha");
			m_red   = ReadValue(reader, "red");
			m_green = ReadValue(reader, "green");
			m_blue  = ReadValue(reader, "blue");
			MakeValue();
		}


		uint ColorVal::WeightedAverageOf(uint aAlpha, uint a, uint bAlpha, uint b, uint cAlpha, uint c, uint dAlpha, uint d)
		{
			uint sumAlpha = aAlpha + bAlpha + cAlpha + dAlpha;
			if (!sumAlpha)
				return AverageOf(a, b, c, d);

			return ((aAlpha * a) + (bAlpha * b) + (cAlpha * c) + (dAlpha * d)) / sumAlpha;
		}


		void ColorVal::Interpolate(ColorVal const& a, ColorVal const& b, ColorVal const& c, ColorVal const& d)
		{
			m_alpha = AverageOf(a.m_alpha, b.m_alpha, c.m_alpha, d.m_alpha );
			m_red   = WeightedAverageOf(a.m_alpha, a.m_red,   b.m_alpha, b.m_red,   c.m_alpha, c.m_red,   d.m_alpha, d.m_red   );
			m_green = WeightedAverageOf(a.m_alpha, a.m_green, b.m_alpha, b.m_green, c.m_alpha, c.m_green, d.m_alpha, d.m_green );
			m_blue  = WeightedAverageOf(a.m_alpha, a.m_blue,  b.m_alpha, b.m_blue,  c.m_alpha, c.m_blue,  d.m_alpha, d.m_blue  );
			MakeValue();
		}


		void ColorVal::MakeValue()
		{
			m_value = (m_alpha << 24) |
					  (m_red   << 16) |
					  (m_green <<  8) |
					  (m_blue       );
		}



		// Gradient

		void Gradient::Read(Seq& reader)
		{
			m_angle = reader.ReadDouble();
			if (isnan(m_angle))											throw DecodeErr(__FUNCTION__ ": Expecting angle");
			if (m_angle < 0.0 || m_angle > 90.0)						throw DecodeErr(__FUNCTION__ ": Unsupported angle");
			if (reader.ReadByte() != ' ')								throw DecodeErr(__FUNCTION__ ": Expecting space after angle");

			m_distanceMin = reader.ReadDouble();
			if (isnan(m_distanceMin))									throw DecodeErr(__FUNCTION__ ": Expecting distance-min");
			if (reader.ReadByte() != ' ')								throw DecodeErr(__FUNCTION__ ": Expecting space after distance-min");

			m_distanceMax = reader.ReadDouble();
			if (isnan(m_distanceMax))									throw DecodeErr(__FUNCTION__ ": Expecting distance-max");
			if (m_distanceMax < m_distanceMin)							throw DecodeErr(__FUNCTION__ ": distance-max is less than distance-min");
			if (reader.ReadByte() != ' ')								throw DecodeErr(__FUNCTION__ ": Expecting space after distance-max");

			m_colorMin.Read(reader);
			if (reader.ReadByte() != ' ')								throw DecodeErr(__FUNCTION__ ": Expecting space after color-min");
			m_colorMax.Read(reader);

			double const pi = acos(-1);
			double perpendicularAngle = 90.0 - m_angle;
			double perpendicularAngleRadians = (perpendicularAngle * pi) / 180.0;
			
			m_lineSin = sin(perpendicularAngleRadians);
			m_lineCos = cos(perpendicularAngleRadians);
		}


		double Gradient::PointDistance(double rowIndex, double colIndex) const
		{
			// For a point (X,Y) and a general line equation Ax + By + C = 0, distance from point to line is: (AX + BY + C) / sqrt(A^2 + B^2):
			// https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
			// In our case, we use a line through the origin point (this makes C = 0) that's perpendicular to the direction of the gradient.
			// Therefore, we have A = sin(90deg - m_angle), B = cos(90deg - m_angle), and A^2 + B^2 = 1. So distance is simple: AX + BY.
			return (m_lineSin * colIndex) + (m_lineCos * rowIndex);
		}


		uint Gradient::ValueWeightedByDistanceFactor(uint distanceFactor, uint minDistVal, uint maxDistVal) const
		{
			return (((MaxDistanceFactor - distanceFactor) * minDistVal) + (distanceFactor * maxDistVal)) / MaxDistanceFactor;
		}


		void Gradient::Apply(ColorVal& result, uint rowIndex, uint colIndex) const
		{
			double distance = PointDistance(rowIndex, colIndex);
			uint distanceFactor;

				 if (distance <= m_distanceMin) distanceFactor = 0;
			else if (distance >= m_distanceMax) distanceFactor = MaxDistanceFactor;
			else                                distanceFactor = (uint) (MaxDistanceFactor * ((distance - m_distanceMin) / (m_distanceMax - m_distanceMin)));

			result.m_alpha = ValueWeightedByDistanceFactor(distanceFactor, m_colorMin.m_alpha, m_colorMax.m_alpha );
			result.m_red   = ValueWeightedByDistanceFactor(distanceFactor, m_colorMin.m_red,   m_colorMax.m_red   );
			result.m_green = ValueWeightedByDistanceFactor(distanceFactor, m_colorMin.m_green, m_colorMax.m_green );
			result.m_blue  = ValueWeightedByDistanceFactor(distanceFactor, m_colorMin.m_blue,  m_colorMax.m_blue  );
			result.MakeValue();
		}



		// Brush

		void Brush::Read(Seq& reader)
		{
			if (reader.StripPrefixExact("color "))
			{
				m_type = BrushType::Color;
				m_color.Set(new ColorVal);
				m_color->Read(reader);
			}
			else if (reader.StripPrefixExact("gradient "))
			{
				m_type = BrushType::Gradient;
				m_gradient.Set(new Gradient);
				m_gradient->Read(reader);
			}
			else
				throw DecodeErr(__FUNCTION__ ": Expecting color or gradient definition");
		}


		void Brush::Apply(ColorVal& result, uint rowIndex, uint colIndex) const
		{
			if (m_type == BrushType::Color)
				m_color->Apply(result);
			else
				m_gradient->Apply(result, rowIndex, colIndex);
		}



		// Icon

		void Icon::Read(Seq& reader, Icon const* prevIcon)
		{
			if (reader.StripPrefixExact("add-half-size"))
			{
				if (!prevIcon)											throw DecodeErr(__FUNCTION__ ": add-half-size: No previous icon");
				
				m_encode = !reader.StripPrefixExact(" no-encode");
				if (!reader.ReadLeadingNewLine().n)						throw DecodeErr(__FUNCTION__ ": add-half-size: Expecting new line");

				MakeHalfSize(*prevIcon);
				return;
			}

			m_width = reader.ReadNrUInt32Dec();
			if (m_width < 1 || m_width > MaxDimension)					throw DecodeErr(__FUNCTION__ ": Invalid width");
			if (!reader.StripPrefixExact("x"))							throw DecodeErr(__FUNCTION__ ": Invalid width and height");

			m_height = reader.ReadNrUInt32Dec();
			if (m_height < 1 || m_height > MaxDimension)				throw DecodeErr(__FUNCTION__ ": Invalid height");
			m_encode = !reader.StripPrefixExact(" no-encode");
			if (!reader.ReadLeadingNewLine().n)							throw DecodeErr(__FUNCTION__ ": Expecting new line after width and height");

			while (reader.StripPrefixExact(":"))
			{
				uint pixelChar = reader.ReadByte();
				if (pixelChar < 32 || pixelChar > 126)					throw DecodeErr(__FUNCTION__ ": color-definition: Expecting pixel-char");
				if (reader.ReadByte() != ' ')							throw DecodeErr(__FUNCTION__ ": color-definition: Expecting space");
				
				Brush& brush = GetBrush(pixelChar);
				brush.Read(reader);

				reader.DropToFirstByteNotOfType(Ascii::IsHorizWhitespace);
				if (!reader.ReadLeadingNewLine().n)						throw DecodeErr(__FUNCTION__ ": color-definition: Expecting new line");
			}

			m_rows.Resize(m_height);
			Brush const& defaultBrush = GetBrush(32);

			for (uint rowIndex=0; rowIndex!=m_height; ++rowIndex)
			{
				Vec<ColorVal>& row = m_rows[rowIndex];
				row.ReserveExact(m_width);

				if (reader.StripPrefixExact("`"))
				{
					for (uint colIndex=0; colIndex!=m_width; ++colIndex)
					{
						if (reader.StartsWithAnyOfType(Ascii::IsVertWhitespace))
							defaultBrush.Apply(row.Add(), rowIndex, colIndex);
						else
						{
							uint pixelChar = reader.ReadByte();
							if (pixelChar < 32 || pixelChar > 126)
								throw DecodeErr(Str(__FUNCTION__ ": pixel-row ").UInt(rowIndex).Add(": X coordinate ").UInt(colIndex).Add(": Expecting pixel-char"));

							uint pixelChar2 = reader.ReadByte();
							if (pixelChar2 != pixelChar)
								throw DecodeErr(Str(__FUNCTION__ ": pixel-row ").UInt(rowIndex).Add(": X coordinate ").UInt(colIndex).Add(": Expecting two same pixel-chars"));

							GetBrush(pixelChar).Apply(row.Add(), rowIndex, colIndex);
						}
					}

					if (!reader.ReadLeadingNewLine().n)
						if (rowIndex != m_height - 1)
							throw DecodeErr(Str(__FUNCTION__ ": pixel-row ").UInt(rowIndex).Add(": Expecting new line"));
				}
			}
		}


		void Icon::MakeHalfSize(Icon const& orig)
		{
			if (orig.m_width != orig.m_height)	throw DecodeErr(__FUNCTION__ ": Original image must have equal width and height");
			if ((orig.m_width % 2) != 0)		throw DecodeErr(__FUNCTION__ ": Original image must have even width and height");

			m_width = m_height = orig.m_width / 2;
			m_rows.Resize(m_height);

			for (uint rowIndex=0; rowIndex!=m_height; ++rowIndex)
			{
				Vec<ColorVal>& row = m_rows[rowIndex];
				row.ReserveExact(m_width);

				for (uint colIndex=0; colIndex!=m_width; ++colIndex)
				{
					ColorVal const& a = orig.m_rows[(2*rowIndex)  ][(2*colIndex)  ];
					ColorVal const& b = orig.m_rows[(2*rowIndex)  ][(2*colIndex)+1];
					ColorVal const& c = orig.m_rows[(2*rowIndex)+1][(2*colIndex)  ];
					ColorVal const& d = orig.m_rows[(2*rowIndex)+1][(2*colIndex)+1];
					row.Add().Interpolate(a, b, c, d);
				}
			}
		}



		// IconAndBmp

		void IconAndBmp::Encode()
		{
			if (!m_encode)
				return;

			sizet allRowsBytes = (4*m_width) * m_height;
			m_bmp.Clear().ReserveExact(40 + allRowsBytes);

			// Encode BITMAPINFOHEADER
			m_bmp	.Word32LE(40)									// sizeof(BITMAPINFOHEADER)
					.Word32LE(m_width)								// Bitmap width in pixels
					.Word32LE(m_height * 2)							// Bitmap height in pixels. Must be doubled for some reason.
					.Word16LE(1)									// Number of planes (1)
					.Word16LE(32)									// Bits per pixel (32)
					.Word32LE(0)									// Compression type = BI_RGB (0)
					.Word32LE(NumCast<uint32>(allRowsBytes))		// Image data size
					.Word32LE(0)									// Horizontal pixels per meter
					.Word32LE(0)									// Vertical pixels per meter
					.Word32LE(0)									// Color indices used
					.Word32LE(0);									// Color indices required/important to display

			// Encode pixels
			for (sizet rowIndex=m_rows.Len(); rowIndex; )
			{
				Vec<ColorVal> const& row = m_rows[--rowIndex];
				for (ColorVal pixel : row)
					m_bmp.Word32LE(pixel.m_value);
			}
		}



		// EncodeIco

		Str EncodeIco(Seq art)
		{
			// Read and encode individual icons
			Vec<IconAndBmp> icons;
			sizet iconsToEncode {};
			while (true)
			{
				art.DropToFirstByteNotOfType(Ascii::IsWhitespace);
				if (!art.n)
					break;

				if (icons.Len() == UINT16_MAX)
					throw DecodeErr(__FUNCTION__ ": Too many icons");

				icons.Add();

				Icon const* prevIcon {};
				if (icons.Len() > 1)
					prevIcon = &(icons[icons.Len() - 2]);

				icons.Last().Read(art, prevIcon);
				if (icons.Last().m_encode)
					++iconsToEncode;
			}

			// Calculate sizes and offsets
			sizet iconDirBytes = 6 + (16 * iconsToEncode);	// ICONDIR and ICONDIRENTRY structures
			sizet firstIconOffset = iconDirBytes;
			
			sizet nextIconOffset = firstIconOffset;
			for (IconAndBmp& icon : icons)
				if (icon.m_encode)
				{
					if (nextIconOffset >= UINT32_MAX)
						throw DecodeErr(__FUNCTION__ ": Result size too large");

					icon.Encode();

					icon.m_fileOffset = (uint32) nextIconOffset;
					nextIconOffset += icon.m_bmp.Len();
				}

			// Allocate space
			Str ico;
			ico.ReserveExact(nextIconOffset);

			// Encode ICONDIR structure
			ico	.Word16LE(0)												// Reserved (0)
				.Word16LE(1)												// Image type (1 = .ICO)
				.Word16LE((uint16) iconsToEncode);							// Number of images contained (1)

			for (IconAndBmp const& icon : icons)
				if (icon.m_encode)
				{
					// Encode ICONDIRENTRY structure and icon
					EnsureThrow(icon.m_width <= 256);
					EnsureThrow(icon.m_height <= 256);

					ico .Byte((byte) icon.m_width)							// 256 width is encoded as 0
						.Byte((byte) icon.m_height)							// 256 height is encoded as 0
						.Byte(0)											// No color palette
						.Byte(0)											// Reserved (0)
						.Word16LE(1)										// Number of color planes
						.Word16LE(16)										// Bits per pixel
						.Word32LE(NumCast<uint32>(icon.m_bmp.Len()))		// Size of image data
						.Word32LE(icon.m_fileOffset);						// Offset of image in file
				}

			for (IconAndBmp const& icon : icons)
				if (icon.m_encode)
					ico.Add(icon.m_bmp);

			return ico;
		}

	}
}
