#pragma once

#include "AtException.h"
#include "AtVec.h"

namespace At
{
	// GeoBuckets divide a sphere (such as Planet Earth) into enumerated sections of similar size.
	// The concept of division is as follows:
	//
	//	                    .
	//                    /___\
	//                  /___|___\
	//                /_____|_____\
	//              /___/___|___\___\
	//            /____/____|____\____\
	//          /_____/_____|_____\_____\
	//        /______/______|______\______\
	//      /___|___/___|___|___|___\___|___\
	//
	// For each hemisphere:
	// - GeoBuckets starting at 0-59 degrees of latitude from the equator are 1 degree of longitude in width. (60*360 = 21,600)
	// - GeoBuckets starting at 60-75 degrees of latitude from the equator are 2 degrees of longitude in width. (16*180 = 2,880)
	// - GeoBuckets starting at 76-82 degrees of latitude from the equator are 4 degrees of longitude in width. (7*90 = 630)
	// - GeoBuckets starting at 83-86 degrees of latitude from the equator are 8 degrees of longitude in width. (4*45 = 180)
	// - There is one GeoBucket starting at 87 degrees of latitude from the equator, spanning the whole longitude, forming a cap.
	// - There are a total of 21,600 + 2,880 + 630 + 180 + 1 = 25,291 GeoBuckets on each hemisphere.


	struct GeoDirection { enum E {
		East	= 1,
		North	= 2,
		West	= 3,
		South	= 4,
	}; };


	struct GeoBucket
	{
		enum { MaxRegularNeighbors = 5 };

		int m_id {};
		double m_latitudeSouth {};		// inclusive
		double m_latitudeNorth {};		// exclusive, except inclusive for point where latitude=90
		double m_longitudeWest {};		// inclusive
		double m_longitudeEast {};		// exclusive

		int m_eastNeighbor {};			// not used for pole buckets
		int m_westNeighbor {};			// not used for pole buckets
	};


	struct AtGeoBucketsByLng
	{
		int m_bucketsByLat[181];	// Index is the bucket's south latitude boundary + 90. Note the extra index for latitude=90
	};


	void ValidateGeoCoords(double latitude, double longitude);

	double GeoDistanceInKm(double latS, double lngS, double latF, double lngF);


	class GeoBuckets : public NoCopy
	{
	public:
		void Init();
	
		int GetBucketContaining(double latitude, double longitude);
		void EnumBucketsNear(Vec<int>& buckets, double latitude, double longitude, double distanceKm);
	
	private:
		Vec<GeoBucket> m_buckets;
		Vec<AtGeoBucketsByLng> m_bucketsByLng;		// Index is the bucket's western longitude boundary + 180
	};
}
