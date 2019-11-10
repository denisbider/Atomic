#include "AtIncludes.h"
#include "AtCountries.h"

namespace At
{
	Seq CountryName_From_Iso3166_1_Alpha2(Seq code)
	{
		if (code.n != 2)
			return "";
	
		byte c1 = ToUpper(code.p[0]);
		byte c2 = ToUpper(code.p[1]);
	
		switch (c1) { case 'A': switch (c2) { case 'C': return "Ascension Island";
											  case 'D': return "Andorra";
											  case 'E': return "United Arab Emirates";
											  case 'F': return "Afghanistan";
											  case 'G': return "Antigua and Barbuda";
											  case 'I': return "Anguilla";
											  case 'L': return "Albania";
											  case 'M': return "Armenia";
											  case 'N': return "Netherlands Antilles";
											  case 'O': return "Angola";
											  case 'Q': return "Antarctica";
											  case 'R': return "Argentina";
											  case 'S': return "American Samoa";
											  case 'T': return "Austria";
											  case 'U': return "Australia";
											  case 'W': return "Aruba";
											  case 'X': return "Aland Islands";
											  case 'Z': return "Azerbaijan";
											  default:  return ""; }
					  case 'B': switch (c2) { case 'A': return "Bosnia and Herzegovina";
											  case 'B': return "Barbados";
											  case 'D': return "Bangladesh";
											  case 'E': return "Belgium";
											  case 'F': return "Burkina Faso";
											  case 'G': return "Bulgaria";
											  case 'H': return "Bahrain";
											  case 'I': return "Burundi";
											  case 'J': return "Benin";
											  case 'L': return "Saint Barthelemy";
											  case 'M': return "Bermuda";
											  case 'N': return "Brunei";
											  case 'O': return "Bolivia";
											  case 'Q': return "Bonaire, Sint Eustatius and Saba";
											  case 'R': return "Brazil";
											  case 'S': return "Bahamas";
											  case 'T': return "Bhutan";
											  case 'U': return "Burma";
											  case 'V': return "Bouvet Island";
											  case 'W': return "Botswana";
											  case 'Y': return "Belarus";
											  case 'Z': return "Belize";
											  default:  return ""; }
					  case 'C': switch (c2) { case 'A': return "Canada";
											  case 'C': return "Cocos (Keeling) Islands";
											  case 'D': return "Democratic Republic of the Congo";
											  case 'F': return "Central African Republic";
											  case 'G': return "Congo";
											  case 'H': return "Switzerland";
											  case 'I': return "Cote d'Ivoire";
											  case 'K': return "Cook Islands";
											  case 'L': return "Chile";
											  case 'M': return "Cameroon";
											  case 'N': return "China";
											  case 'O': return "Colombia";
											  case 'P': return "Clipperton Island";
											  case 'R': return "Costa Rica";
											  case 'S': return "Serbia and Montenegro";
											  case 'U': return "Cuba";
											  case 'V': return "Cape Verde";
											  case 'W': return "Curacao";
											  case 'X': return "Christmas Island";
											  case 'Y': return "Cyprus";
											  case 'Z': return "Czech Republic";
											  default:  return ""; }
					  case 'D': switch (c2) { case 'E': return "Germany";
											  case 'G': return "Diego Garcia";
											  case 'J': return "Djibouti";
											  case 'K': return "Denmark";
											  case 'M': return "Dominica";
											  case 'O': return "Dominican Republic";
											  case 'Y': return "Benin";
											  case 'Z': return "Algeria";
											  default:  return ""; }
					  case 'E': switch (c2) { case 'A': return "Ceuta, Melilla";
											  case 'C': return "Ecuador";
											  case 'E': return "Estonia";
											  case 'G': return "Egypt";
											  case 'H': return "Western Sahara";
											  case 'R': return "Eritrea";
											  case 'S': return "Spain";
											  case 'T': return "Ethiopia";
											  case 'U': return "European Union";
											  case 'W': return "Estonia";
											  default:  return ""; }
					  case 'F': switch (c2) { case 'I': return "Finland";
											  case 'J': return "Fiji";
											  case 'K': return "Falkland Islands";
											  case 'L': return "Liechtenstein";
											  case 'M': return "Micronesia";
											  case 'O': return "Faroe Islands";
											  case 'R': return "France";
											  case 'X': return "France, Metropolitan";
											  default:  return ""; }
					  case 'G': switch (c2) { case 'A': return "Gabon";
											  case 'B': return "United Kingdom";
											  case 'D': return "Grenada";
											  case 'E': return "Georgia";
											  case 'F': return "French Guiana";
											  case 'G': return "Guernsey";
											  case 'H': return "Ghana";
											  case 'I': return "Gibraltar";
											  case 'L': return "Greenland";
											  case 'M': return "Gambia";
											  case 'N': return "Guinea";
											  case 'P': return "Guadeloupe";
											  case 'Q': return "Equatorial Guinea";
											  case 'R': return "Greece";
											  case 'S': return "South Georgia and the South Sandwich Islands";
											  case 'T': return "Guatemala";
											  case 'U': return "Guam";
											  case 'W': return "Guinea-Bissau";
											  case 'Y': return "Guyana";
											  default:  return ""; }
					  case 'H': switch (c2) { case 'K': return "Hong Kong";
											  case 'M': return "Heard Island and McDonald Islands";
											  case 'N': return "Honduras";
											  case 'R': return "Croatia";
											  case 'T': return "Haiti";
											  case 'U': return "Hungary";
											  default:  return ""; }
					  case 'I': switch (c2) { case 'C': return "Canary Island";
											  case 'D': return "Indonesia";
											  case 'E': return "Ireland";
											  case 'L': return "Israel";
											  case 'M': return "Isle of Man";
											  case 'N': return "India";
											  case 'O': return "British Indian Ocean Territory";
											  case 'Q': return "Iraq";
											  case 'R': return "Iran";
											  case 'S': return "Iceland";
											  case 'T': return "Italy";
											  default:  return ""; }
					  case 'J': switch (c2) { case 'A': return "Jamaica";
											  case 'E': return "Jersey";
											  case 'M': return "Jamaica";
											  case 'O': return "Jordan";
											  case 'P': return "Japan";
											  default:  return ""; }
					  case 'K': switch (c2) { case 'E': return "Kenya";
											  case 'G': return "Kyrgyzstan";
											  case 'H': return "Cambodia";
											  case 'I': return "Kiribati";
											  case 'M': return "Comoros";
											  case 'N': return "Saint Kitts and Nevis";
											  case 'P': return "North Korea";
											  case 'R': return "South Korea";
											  case 'W': return "Kuwait";
											  case 'Y': return "Cayman Islands";
											  case 'Z': return "Kazakhstan";
											  default:  return ""; }
					  case 'L': switch (c2) { case 'A': return "Laos";
											  case 'B': return "Lebanon";
											  case 'C': return "Saint Lucia";
											  case 'F': return "Libya Fezzan";
											  case 'I': return "Liechtenstein";
											  case 'K': return "Sri Lanka";
											  case 'R': return "Liberia";
											  case 'S': return "Lesotho";
											  case 'T': return "Lithuania";
											  case 'U': return "Luxembourg";
											  case 'V': return "Latvia";
											  case 'Y': return "Libya";
											  default:  return ""; }
					  case 'M': switch (c2) { case 'A': return "Morocco";
											  case 'C': return "Monaco";
											  case 'D': return "Moldova";
											  case 'E': return "Montenegro";
											  case 'F': return "Saint Martin (French)";
											  case 'G': return "Madagascar";
											  case 'H': return "Marshall Islands";
											  case 'K': return "Macedonia";
											  case 'L': return "Mali";
											  case 'M': return "Myanmar";
											  case 'N': return "Mongolia";
											  case 'O': return "Macao";
											  case 'P': return "Northern Mariana Islands";
											  case 'Q': return "Martinique";
											  case 'R': return "Mauritania";
											  case 'S': return "Montserrat";
											  case 'T': return "Malta";
											  case 'U': return "Mauritius";
											  case 'V': return "Maldives";
											  case 'W': return "Malawi";
											  case 'X': return "Mexico";
											  case 'Y': return "Malaysia";
											  case 'Z': return "Mozambique";
											  default:  return ""; }
					  case 'N': switch (c2) { case 'A': return "Namibia";
											  case 'C': return "New Caledonia";
											  case 'E': return "Niger";
											  case 'F': return "Norfolk Island";
											  case 'G': return "Nigeria";
											  case 'I': return "Nicaragua";
											  case 'L': return "Netherlands";
											  case 'O': return "Norway";
											  case 'P': return "Nepal";
											  case 'R': return "Nauru";
											  case 'T': return "Neutral Zone";
											  case 'U': return "Niue";
											  case 'Z': return "New Zealand";
											  default:  return ""; }
					  case 'O': switch (c2) { case 'M': return "Oman";
											  default:  return ""; }
					  case 'P': switch (c2) { case 'A': return "Panama";
											  case 'E': return "Peru";
											  case 'F': return "French Polynesia";
											  case 'G': return "Papua New Guinea";
											  case 'H': return "Philippines";
											  case 'I': return "Philippines";
											  case 'K': return "Pakistan";
											  case 'L': return "Poland";
											  case 'M': return "Saint Pierre and Miquelon";
											  case 'N': return "Pitcairn";
											  case 'R': return "Puerto Rico";
											  case 'S': return "Palestinian Territory";
											  case 'T': return "Portugal";
											  case 'W': return "Palau";
											  case 'Y': return "Paraguay";
											  default:  return ""; }
					  case 'Q': switch (c2) { case 'A': return "Qatar";
											  default:  return ""; }
					  case 'R': switch (c2) { case 'A': return "Argentina";
											  case 'B': return "Bolivia";
											  case 'C': return "China";
											  case 'E': return "Reunion";
											  case 'H': return "Haiti";
											  case 'I': return "Indonesia";
											  case 'L': return "Lebanon";
											  case 'M': return "Madagascar";
											  case 'N': return "Niger";
											  case 'O': return "Romania";
											  case 'P': return "Philippines";
											  case 'S': return "Serbia";
											  case 'U': return "Russian Federation";
											  case 'W': return "Rwanda";
											  default:  return ""; }
					  case 'S': switch (c2) { case 'A': return "Saudi Arabia";
											  case 'B': return "Solomon Islands";
											  case 'C': return "Seychelles";
											  case 'D': return "Sudan";
											  case 'E': return "Sweden";
											  case 'F': return "Finland";
											  case 'G': return "Singapore";
											  case 'H': return "Saint Helena, Ascension and Tristan da Cunha";
											  case 'I': return "Slovenia";
											  case 'J': return "Svalbard and Jan Mayen";
											  case 'K': return "Slovakia";
											  case 'L': return "Sierra Leone";
											  case 'M': return "San Marino";
											  case 'N': return "Senegal";
											  case 'O': return "Somalia";
											  case 'R': return "Suriname";
											  case 'S': return "South Sudan";
											  case 'T': return "Sao Tome and Principe";
											  case 'U': return "USSR";
											  case 'V': return "El Salvador";
											  case 'X': return "Sint Maarten (Dutch)";
											  case 'Y': return "Syria";
											  case 'Z': return "Swaziland";
											  default:  return ""; }
					  case 'T': switch (c2) { case 'A': return "Tristan da Cunha";
											  case 'C': return "Turks and Caicos Islands";
											  case 'D': return "Chad";
											  case 'F': return "French Southern Territories";
											  case 'G': return "Togo";
											  case 'H': return "Thailand";
											  case 'J': return "Tajikistan";
											  case 'K': return "Tokelau";
											  case 'L': return "Timor-Leste";
											  case 'M': return "Turkmenistan";
											  case 'N': return "Tunisia";
											  case 'O': return "Tonga";
											  case 'P': return "East Timor";
											  case 'R': return "Turkey";
											  case 'T': return "Trinidad and Tobago";
											  case 'V': return "Tuvalu";
											  case 'W': return "Taiwan";
											  case 'Z': return "Tanzania";
											  default:  return ""; }
					  case 'U': switch (c2) { case 'A': return "Ukraine";
											  case 'G': return "Uganda";
											  case 'K': return "United Kingdom";
											  case 'M': return "U.S. Minor Outlying Islands";
											  case 'S': return "United States";
											  case 'Y': return "Uruguay";
											  case 'Z': return "Uzbekistan";
											  default:  return ""; }
					  case 'V': switch (c2) { case 'A': return "Vatican";
											  case 'C': return "Saint Vincent and the Grenadines";
											  case 'E': return "Venezuela";
											  case 'G': return "British Virgin Islands";
											  case 'I': return "U.S. Virgin Islands";
											  case 'N': return "Vietnam";
											  case 'U': return "Vanuatu";
											  default:  return ""; }
					  case 'W': switch (c2) { case 'F': return "Wallis and Futuna";
											  case 'G': return "Grenada";
											  case 'L': return "Saint Lucia";
											  case 'V': return "Saint Vincent";
											  case 'S': return "Samoa";
											  default:  return ""; }
					  case 'Y': switch (c2) { case 'E': return "Yemen";
											  case 'T': return "Mayotte";
											  case 'U': return "Yugoslavia";
											  case 'V': return "Venezuela";
											  default:  return ""; }
					  case 'Z': switch (c2) { case 'A': return "South Africa";
											  case 'M': return "Zambia";
											  case 'R': return "Zaire";
											  case 'W': return "Zimbabwe";
											  default:  return ""; }
					  default:                          return ""; }
	}


	Seq RegionName_US(Seq code)
	{
		if (code.n != 2)
			return "";
	
		byte c1 = ToUpper(code.p[0]);
		byte c2 = ToUpper(code.p[1]);
	
		switch (c1) { case 'A': switch (c2) { case 'K': return "Alaska";
											  case 'L': return "Alabama";
											  case 'R': return "Arkansas";
											  case 'S': return "American Samoa";
											  case 'Z': return "Arizona";
											  default:  return ""; }
					  case 'C': switch (c2) { case 'A': return "California";
											  case 'O': return "Colorado";
											  case 'T': return "Connecticut";
											  default:  return ""; }
					  case 'D': switch (c2) { case 'E': return "Delaware";
											  case 'C': return "District of Columbia";
											  default:  return ""; }
					  case 'F': switch (c2) { case 'L': return "Florida";
											  default:  return ""; }
					  case 'G': switch (c2) { case 'A': return "Georgia";
											  case 'U': return "Guam";
											  default:  return ""; }
					  case 'H': switch (c2) { case 'H': return "Hawaii";
											  default:  return ""; }
					  case 'I': switch (c2) { case 'A': return "Iowa";
											  case 'D': return "Idaho";
											  case 'L': return "Illinois";
											  case 'N': return "Indiana";
											  default:  return ""; }
					  case 'K': switch (c2) { case 'S': return "Kansas";
											  case 'Y': return "Kentucky";
											  default:  return ""; }
					  case 'L': switch (c2) { case 'A': return "Louisiana";
											  default:  return ""; }
					  case 'M': switch (c2) { case 'A': return "Massachusetts";
											  case 'D': return "Maryland";
											  case 'E': return "Maine";
											  case 'I': return "Michigan";
											  case 'N': return "Minnesota";
											  case 'P': return "Northern Mariana Islands";
											  case 'S': return "Mississippi";
											  case 'O': return "Missouri";
											  case 'T': return "Montana";
											  default:  return ""; }
					  case 'N': switch (c2) { case 'C': return "North Carolina";
											  case 'D': return "North Dakota";
											  case 'E': return "Nebraska";
											  case 'H': return "New Hampshire";
											  case 'J': return "New Jersey";
											  case 'M': return "New Mexico";
											  case 'V': return "Nevada";
											  case 'Y': return "New York";
											  default:  return ""; }
					  case 'O': switch (c2) { case 'H': return "Ohio";
											  case 'K': return "Oklahoma";
											  case 'R': return "Oregon";
											  default:  return ""; }
					  case 'P': switch (c2) { case 'A': return "Pennsylvania";
											  case 'R': return "Puerto Rico";
											  default:  return ""; }
					  case 'R': switch (c2) { case 'I': return "Rhode Island";
											  default:  return ""; }
					  case 'S': switch (c2) { case 'C': return "South Carolina";
											  case 'D': return "South Dakota";
											  default:  return ""; }
					  case 'T': switch (c2) { case 'N': return "Tennessee";
											  case 'X': return "Texas";
											  default:  return ""; }
					  case 'U': switch (c2) { case 'T': return "Utah";
											  default:  return ""; }
					  case 'V': switch (c2) { case 'T': return "Vermont";
											  case 'I': return "Virgin Islands";
											  default:  return ""; }
					  case 'W': switch (c2) { case 'A': return "Washington";
											  case 'I': return "Wisconsin";
											  case 'V': return "West Virginia";
											  case 'Y': return "Wyoming";
											  default:  return ""; }
					  default:                          return ""; }
	}


	Seq RegionName_Canada(Seq code)
	{
		if (code == "AB") return "Alberta";
		if (code == "BC") return "British Columbia";
		if (code == "MB") return "Manitoba";
		if (code == "NB") return "New Brunswick";
		if (code == "NL") return "Newfoundland and Labrador";
		if (code == "NS") return "Nova Scotia";
		if (code == "NT") return "Northwest Territories";
		if (code == "NU") return "Nunavut";
		if (code == "ON") return "Ontario";
		if (code == "PE") return "Prince Edward Island";
		if (code == "QC") return "Quebec";
		if (code == "SK") return "Saskatchewan";
		if (code == "YT") return "Yukon";
						  return "";
	}


	Seq CountryRegionName(Seq countryCode, Seq regionCode)
	{
		if (countryCode == "US") return RegionName_US    (regionCode);
		if (countryCode == "CA") return RegionName_Canada(regionCode);
								 return "";
	}
}
