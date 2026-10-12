#pragma once

#include <array>
#include <cstddef>

namespace Globe {

enum class Marker { Pin, Heart, Rutgers };

struct Location {
    const char* name;
    double latitude;
    double longitude;
    Marker marker = Marker::Pin;
};

inline constexpr auto locations = std::to_array<Location>({
    {"Edgewater, New Jersey", 40.827, -73.975, Marker::Heart},
    {"Rutgers, New Brunswick", 40.499, -74.446, Marker::Rutgers},
    {"New York, New York", 40.713, -74.006},
    {"Miami, Florida", 25.762, -80.192},
    {"Fort Lauderdale, Florida", 26.122, -80.137},
    {"Clearwater, Florida", 27.966, -82.800},
    {"West Palm Beach, Florida", 26.715, -80.053},
    {"Atlanta, Georgia", 33.749, -84.388},
    {"Washington, DC", 38.907, -77.037},
    {"New Haven, Connecticut", 41.308, -72.928},
    {"Providence, Rhode Island", 41.824, -71.413},
    {"Stratton, Vermont", 43.114, -72.907},
    {"Killington, Vermont", 43.604, -72.821},
    {"Montreal, Canada", 45.502, -73.567},
    {"Niagara Falls, Canada", 43.090, -79.084},
    {"Boulder, Colorado", 40.015, -105.271},
    {"Arapahoe Basin, Colorado", 39.643, -105.873},
    {"Las Vegas, Nevada", 36.170, -115.140},
    {"Los Angeles, California", 34.052, -118.244},
    {"San Diego, California", 32.716, -117.161},
    {"Cancun, Mexico", 21.162, -86.852},
    {"Punta Cana, Dominican Republic", 18.582, -68.405},
    {"Nassau, The Bahamas", 25.044, -77.350},
    {"St. Kitts", 17.302, -62.717},
    {"Marrakesh, Morocco", 31.630, -7.981},
    {"Tinghir, Morocco", 31.515, -5.533},
    {"Merzouga Desert, Morocco", 31.099, -4.012},
    {"Lisbon, Portugal", 38.722, -9.139},
    {"Albufeira, Portugal", 37.089, -8.247},
    {"Seville, Spain", 37.389, -5.984},
    {"Madrid, Spain", 40.417, -3.704},
    {"Barcelona, Spain", 41.385, 2.173},
    {"Nice, France", 43.710, 7.262},
    {"Chamonix, France", 45.923, 6.870},
    {"Monaco", 43.738, 7.425},
    {"Paris, France", 48.857, 2.352},
    {"Lugano, Switzerland", 46.004, 8.951},
    {"Brussels, Belgium", 50.850, 4.352},
    {"Amsterdam, Netherlands", 52.367, 4.904},
    {"Munich, Germany", 48.135, 11.582},
    {"Prague, Czechia", 50.075, 14.438},
    {"Copenhagen, Denmark", 55.676, 12.568},
    {"Krakow, Poland", 50.065, 19.945},
    {"Narvik, Norway", 68.438, 17.427},
    {"Abisko, Sweden", 68.349, 18.831},
    {"Kiruna, Sweden", 67.856, 20.225},
    {"London, United Kingdom", 51.507, -0.128},
    {"Cork, Ireland", 51.899, -8.475},
    {"Dublin, Ireland", 53.350, -6.260},
    {"Galway, Ireland", 53.270, -9.056},
    {"Limerick, Ireland", 52.663, -8.626},
    {"Vienna, Austria", 48.208, 16.374},
    {"Bratislava, Slovakia", 48.148, 17.108},
    {"Ljubljana, Slovenia", 46.057, 14.505},
    {"Lake Bled, Slovenia", 46.363, 14.094},
    {"Lake Bohinj, Slovenia", 46.282, 13.858},
    {"Budapest, Hungary", 47.498, 19.040},
    {"Bucharest, Romania", 44.427, 26.103},
    {"Zagreb, Croatia", 45.815, 15.982},
    {"Athens, Greece", 37.984, 23.728},
    {"Thessaloniki, Greece", 40.641, 22.944},
    {"Mount Olympus, Greece", 40.085, 22.358},
    {"Istanbul, Turkey", 41.008, 28.978},
    {"Bodrum, Turkey", 37.034, 27.430},
    {"Fethiye, Turkey", 36.622, 29.116},
    {"Nicosia, Cyprus (south)", 35.170, 33.361},
    {"Nicosia, Cyprus (north)", 35.178, 33.362},
    {"Yerevan, Armenia", 40.187, 44.515},
    {"Gyumri, Armenia", 40.789, 43.847},
    {"Milan, Italy", 45.464, 9.190},
    {"Florence, Italy", 43.770, 11.256},
    {"Venice, Italy", 45.440, 12.315},
    {"Canazei, Italy", 46.477, 11.770},
    {"Bologna, Italy", 44.495, 11.343},
    {"Cinque Terre, Italy", 44.127, 9.710},
    {"Rome, Italy", 41.903, 12.496},
    {"Palermo, Italy", 38.116, 13.361},
    {"Catania, Italy", 37.508, 15.083},
    {"Mount Etna, Italy", 37.751, 14.993},
    {"Taormina, Italy", 37.852, 15.287}
});

}
