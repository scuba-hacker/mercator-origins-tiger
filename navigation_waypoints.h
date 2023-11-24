#ifndef navigation_waypoints_h
#define navigation_waypoints_h

#include <stdint.h>

class navigationWaypoint
{
  public:
    const char*  _label;
    double _lat;
    double _long;

    navigationWaypoint()
    {
      _label = 0;
      _lat = _long = 0.0;
    }

    navigationWaypoint(const char*  label, double latitude, double longitude) : _label(label), _lat(latitude), _long(longitude)
    {
    }
};

extern const uint8_t waypointCount;
extern const navigationWaypoint waypoints[];
extern uint8_t getWaypointsLength();

#endif
