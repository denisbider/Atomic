#include "AtIncludes.h"
#include "AtGeoBuckets.h"

#define _USE_MATH_DEFINES
#include <math.h>

namespace At
{
	double const MeanEarthRadiusKm = 6371;
	double const MeanEarthCircumferenceKm = MeanEarthRadiusKm * M_PI * 2;
	double const OneLatitudeDegreeKm = MeanEarthRadiusKm * (M_PI / 180);
	double const HiEstimateGeoBucketAreaKm2 = OneLatitudeDegreeKm * OneLatitudeDegreeKm;
	double const LoEstimateGeoBucketAreaKm2 = OneLatitudeDegreeKm * OneLatitudeDegreeKm / 2;


	void ValidateGeoCoords(double latitude, double longitude)
	{
		if (latitude < -90 || latitude > 90)
			throw ZLitErr("ValidateGeoCoords: Invalid latitude");
	
		if (longitude < -180 || longitude >= 180)
			throw ZLitErr("ValidateGeoCoords: Invalid longitude");
	}


	double GeoDistanceInKm(double latS, double lngS, double latF, double lngF)
	{
		ValidateGeoCoords(latS, lngS);
		ValidateGeoCoords(latF, lngF);

		// The following algorithm uses the spherical special case of the Vincenty formula for ellipsoids,
		// which is said to be accurate for all distances:
		//
		// http://en.wikipedia.org/w/index.php?title=Great-circle_distance&oldid=429475514
		//
		// The article describes simpler formulae, but the law of cosines is numerically inaccurate
		// for points close together, and the haversine formula for opposite points on a sphere.
	
		double latSRad = (latS * M_PI) / 180;
		double lngSRad = (lngS * M_PI) / 180;
		double latFRad = (latF * M_PI) / 180;
		double lngFRad = (lngF * M_PI) / 180;
		double dLngRad = lngFRad - lngSRad;
		double sinLatS = sin(latSRad);
		double cosLatS = cos(latSRad);
		double sinLatF = sin(latFRad);
		double cosLatF = cos(latFRad);
		double sinDLng = sin(dLngRad);
		double cosDLng = cos(dLngRad);
		double numTerm1 = cosLatF * sinDLng;
		double numTerm2 = (cosLatS * sinLatF) - (sinLatS * cosLatF * cosDLng);
		double numerator = sqrt((numTerm1 * numTerm1) + (numTerm2 * numTerm2));
		double denominator = (sinLatS * sinLatF) + (cosLatS * cosLatF * cosDLng);
		double centralAngle = atan2(numerator, denominator);	
		return fabs(centralAngle * MeanEarthRadiusKm);
	}


	void GeoBuckets::Init()
	{
		m_buckets.ReserveExact(2*25291);
		m_bucketsByLng.ResizeExact(360);

		int id {};
		for (int southHem=0; southHem!=2; ++southHem)
		{
			int firstIdPrevLat { -1 };
			int widthPrevLat { -1 };
		
			for (int lat=0; lat!=88; ++lat)
			{
				bool isPole {};
				int width;
				if (lat < 60)		width = 1;
				else if (lat < 76)	width = 2;
				else if (lat < 83)	width = 4;
				else if (lat < 87)	width = 8;
				else			  { width = 360; isPole = true; }
			
				int firstIdThisLat { id };
				for (int lng=-180; lng!=180; ++lng)
				{
					EnsureThrow(m_buckets.Cap() > m_buckets.Len());

					GeoBucket& bucket { m_buckets.Add() };
					bucket.m_id = 0;
					bucket.m_longitudeWest = lng;
					bucket.m_longitudeEast = lng + width;
					if (southHem)
					{
						bucket.m_latitudeNorth = (-lat);
						if (lat < 87)
							bucket.m_latitudeSouth = (-(lat+1));
						else
							bucket.m_latitudeSouth = -90;
					}
					else
					{
						bucket.m_latitudeSouth = lat;
						if (lat < 87)
							bucket.m_latitudeNorth = lat+1;
						else
							bucket.m_latitudeNorth = 90;
					}
				
					int bucketsByLatIdx { If(southHem, int, (90 - lat - 1), (lat + 90)) };
					for (int bucketsByLngIdx=lng+180; bucketsByLngIdx!=lng+180+width; ++bucketsByLngIdx)
					{
						m_bucketsByLng[(sizet) bucketsByLngIdx].m_bucketsByLat[(sizet) bucketsByLatIdx] = id;

						if (bucketsByLatIdx == 2)
						{
							m_bucketsByLng[(sizet) bucketsByLngIdx].m_bucketsByLat[0] = id;
							m_bucketsByLng[(sizet) bucketsByLngIdx].m_bucketsByLat[1] = id;
						}
						else if (bucketsByLatIdx == 177)
						{
							m_bucketsByLng[(sizet) bucketsByLngIdx].m_bucketsByLat[178] = id;
							m_bucketsByLng[(sizet) bucketsByLngIdx].m_bucketsByLat[179] = id;
							m_bucketsByLng[(sizet) bucketsByLngIdx].m_bucketsByLat[180] = id;
						}
					}

					// Add neighbors
					if (isPole)
					{
						bucket.m_eastNeighbor = -1;
						bucket.m_westNeighbor = -1;
					}
					else
					{
						if (lng > -180)
						{
							m_buckets[(sizet) (id-1)].m_eastNeighbor = id;
							bucket.m_westNeighbor = id - 1;
						
							if (lng == 180)
							{
								bucket.m_eastNeighbor = firstIdThisLat;
								m_buckets[(sizet) firstIdThisLat].m_westNeighbor = id;
							}
						}
					}

					EnsureThrow(((sizet) id) == m_buckets.Len());				
					++id;
				}

				firstIdPrevLat = firstIdThisLat;
				widthPrevLat = width;
			}
		}
	
		EnsureThrow(m_buckets.Len() == 2*25291);
	}


	int GeoBuckets::GetBucketContaining(double latitude, double longitude)
	{
		ValidateGeoCoords(latitude, longitude);
		int lat { (int) floor(latitude) };
		int lng { (int) floor(longitude) };
		return m_bucketsByLng[(sizet) (lng+180)].m_bucketsByLat[(sizet) (lat+90)];
	}


	void GeoBuckets::EnumBucketsNear(Vec<int>& buckets, double latitude, double longitude, double distanceKm)
	{
		double searchAreaKm2 = M_PI * distanceKm * distanceKm;
		sizet estimatedBuckets = (sizet) (2 * (searchAreaKm2 / LoEstimateGeoBucketAreaKm2));
		if (estimatedBuckets < 4)
			estimatedBuckets = 4;
		else if (estimatedBuckets > m_buckets.Len())
			estimatedBuckets = m_buckets.Len();

		buckets.Clear();
		buckets.ReserveExact(estimatedBuckets);
	
		double latitudeSpan = distanceKm / OneLatitudeDegreeKm;
		int northmostLatIdx = ((int) floor(latitude + latitudeSpan)) + 90;
		if (northmostLatIdx >= 177)
		{
			GeoBucket const& poleBucket = m_buckets[(sizet) (m_bucketsByLng[0].m_bucketsByLat[189])];
			buckets.Add(poleBucket.m_id);
			northmostLatIdx = 176;
		}
	
		int southmostLatIdx = (int) floor(latitude - latitudeSpan);
		if (southmostLatIdx < 3)
		{
			GeoBucket const& poleBucket = m_buckets[(sizet) (m_bucketsByLng[0].m_bucketsByLat[1])];
			buckets.Add(poleBucket.m_id);
			southmostLatIdx = 3;
		}
	
		int midLngIdx = ((int) floor(longitude)) + 180;
		for (int curLatIdx=southmostLatIdx; curLatIdx<=northmostLatIdx; ++curLatIdx)
		{
			GeoBucket const& midBucket = m_buckets[(sizet) (m_bucketsByLng[(sizet) midLngIdx].m_bucketsByLat[(sizet) curLatIdx])];
			buckets.Add(midBucket.m_id);
		
			double latDegsFromSearchPoint;
			if (midBucket.m_latitudeNorth < latitude)
				latDegsFromSearchPoint = latitude - midBucket.m_latitudeNorth;
			else if (midBucket.m_latitudeSouth > latitude)
				latDegsFromSearchPoint = midBucket.m_latitudeSouth - latitude;
			else
				latDegsFromSearchPoint = 0;
		
			double kmFromSearchPoint = latDegsFromSearchPoint * OneLatitudeDegreeKm;
			double horizScanKm = sqrt((distanceKm * distanceKm) - (kmFromSearchPoint * kmFromSearchPoint));
			double midBucketLatDeg = (midBucket.m_latitudeNorth + midBucket.m_latitudeSouth) / 2;
			double midBucketLatRad = (midBucketLatDeg * M_PI) / 180;
			double latitudeCircumferenceKm = MeanEarthCircumferenceKm * cos(fabs(midBucketLatRad));
			double horizScanDeg = 360 * (horizScanKm / latitudeCircumferenceKm);
			if (horizScanDeg > 180)
				horizScanDeg = 180;
		
			double bucketWidthDeg = midBucket.m_longitudeEast - midBucket.m_longitudeWest;
			int neighborsToAdd = (int) ceil(horizScanDeg / bucketWidthDeg);		
		
			// Add buckets to the east
			int lastEastBucketId = midBucket.m_id;
			GeoBucket const* curBucket = &midBucket;
			for (int neighborsAdded=0; neighborsAdded!=neighborsToAdd; ++neighborsAdded)
			{
				curBucket = &m_buckets[(sizet) (curBucket->m_eastNeighbor)];
				buckets.Add(curBucket->m_id);
				lastEastBucketId = curBucket->m_id;
			}
		
			// Add buckets to the west
			curBucket = &midBucket;
			for (int neighborsAdded=0; neighborsAdded!=neighborsToAdd; ++neighborsAdded)
			{
				curBucket = &m_buckets[(sizet) (curBucket->m_westNeighbor)];
				if (curBucket->m_id == lastEastBucketId)
					break;
				buckets.Add(curBucket->m_id);
			}
		}
	}
}
