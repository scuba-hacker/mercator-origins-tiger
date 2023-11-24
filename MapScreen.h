#ifndef mapscreen_h
#define mapscreen_h

#include <stdint.h>
#include <memory>

class M5StickCPlus;
class TFT_eSPI;
class TFT_eSprite;
class navigationWaypoint;


class geo_map
{
  public:
  
    const uint16_t* mapData;
    const char* label;
    const uint16_t backColour;
    const char* backText;
    const bool surveyMap;
    const bool swapBytes;
    const float mapLongitudeLeft;
    const float mapLongitudeRight;
    const float mapLatitudeBottom;
  
    geo_map() : mapData(0),backColour(0),backText((const char*)0), surveyMap(false),swapBytes(false), mapLongitudeLeft(0),mapLongitudeRight(0),mapLatitudeBottom(0)
    {}
    
    geo_map(const uint16_t*  md, const char* l, uint16_t bc,const char* bt, bool sm, bool sb, float ll, float lr, float lb) : mapData(md),label(l),backColour(bc),backText(bt),surveyMap(sm),swapBytes(sb),mapLongitudeLeft(ll),mapLongitudeRight(lr),mapLatitudeBottom(lb)
    {}
};

class geoRef
{
  public:
    int geoMaps[4];
};

/* Requirements:
 *  
 *  DONE - survey maps are checked for presence before non-survey maps.
 *  DONE - survey maps show all features within map extent, plus diver, without last visited feature shown.
 *  DONE - survey maps only switch to last zoom non-survey map when out of area.
 *  DONE - diver sprite is rotated according to compass direction.
 *  DONE - at zoom level 1 and 2, base map gets switched once the selectMap function has detected diver moved to edge as defined by current map.
 *  DONE - at zoom level 2 switch between tiles within a base map is done at a tile boundary.
 *  DONE - for non-survey maps, last feature and next feature are shown in different colours.
 *  DONE - Heading indicator in blue
 *  DONE - Direction Line in red to next feature spanning maps and tiles at any zoom
 *  DONE - Green Line pointing to nearest exit Cafe or Mid Jetty
 *  TODO - flash Diver Sprite Pink/Yellow when recording a new PIN location.
 *  TODO - Diver sprite flashes blue/green.
 *  TODO - breadcrumb trail
 */
 
class MapScreen
{     
  static const geo_map s_maps[];
 
  public:
    class pixel
    {
      public:
        pixel() : x(0), y(0), colour(0) {}
        pixel(int16_t xx, int16_t yy, uint16_t colourr) : x(xx), y(yy), colour(colourr) {}
        pixel(int16_t xx, int16_t yy) : x(xx), y(yy), colour(0) {}

        int16_t x;
        int16_t y;
        uint16_t colour;
    };

  public:
    MapScreen(TFT_eSPI* tft, M5StickCPlus* m5);
    ~MapScreen()
    {

    }
    
    void setTargetWaypointByLabel(const char* label);

    void setUseDiverHeading(const bool use)
    {
      _useDiverHeading = use;
    }
    
    void initCurrentMap(const double diverLatitude, const double diverLongitude);

    void drawPlaceholderMap();
        
    void clearMap();
    void clearMapPreserveZoom();
    
    void drawFeaturesOnSpecifiedMapToScreen(const geo_map* featureAreaToShow, int16_t zoom=1, int16_t tileX=0, int16_t tileY=0);

    void drawDiverOnBestFeaturesMapAtCurrentZoom(const double diverLatitude, const double diverLongitude, const double diverHeading = 0);
    
    void drawDiverOnCompositedMapSprite(const double latitude, const double longitude, const double heading, const geo_map* featureMap);

    double distanceBetween(double lat1, double long1, double lat2, double long2) const;
    double degreesCourseTo(double lat1, double long1, double lat2, double long2) const;
    double radiansCourseTo(double lat1, double long1, double lat2, double long2) const;


    const navigationWaypoint* getClosestJetty(double& distance);

    int drawDirectionLineOnCompositeSprite(const double diverLatitude, const double diverLongitude, 
                                                    const geo_map* featureMap, const navigationWaypoint* waypoint, uint16_t colour, int indicatorLength);

    void drawHeadingLineOnCompositeMapSprite(const double diverLatitude, const double diverLongitude, 
                                            const double heading, const geo_map* featureMap);

    void drawRegistrationPixelsOnCleanMapSprite(const geo_map* featureMap);

    void cycleZoom();

    
    bool isAllLakeShown() const { return _showAllLake; }
    void setAllLakeShown(bool showAll);
    void toggleAllLakeShown()
    {
      setAllLakeShown(!isAllLakeShown());
    }

    int16_t getZoom() const     { return _zoom; }
    void setZoom(const int16_t zoom);

    void setDrawAllFeatures(const bool showAll)
    { 
      _drawAllFeatures = showAll; 
      _currentMap = nullptr;     // invalidate current map
    }

    void toggleDrawAllFeatures()
    {
      setDrawAllFeatures(!getDrawAllFeatures());
    }

    bool getDrawAllFeatures() const
    { return _drawAllFeatures; }

    void testAnimatingDiverSpriteOnCurrentMap();
    void testDrawingMapsAndFeatures(uint8_t& currentMap, int16_t& zoom);

  private:
    TFT_eSPI* _tft;
    M5StickCPlus* _m5;
    
    std::unique_ptr<TFT_eSprite> _cleanMapAndFeaturesSprite;
    std::unique_ptr<TFT_eSprite> _compositedScreenSprite;
    std::unique_ptr<TFT_eSprite> _diverSprite;
    std::unique_ptr<TFT_eSprite> _diverPlainSprite;
    std::unique_ptr<TFT_eSprite> _diverRotatedSprite;
    std::unique_ptr<TFT_eSprite> _featureSprite;
    std::unique_ptr<TFT_eSprite> _targetSprite;
    std::unique_ptr<TFT_eSprite> _lastTargetSprite;

    std::unique_ptr<geoRef[]>    _featureToMaps;

    static const int16_t s_imgHeight = 240;
    static const int16_t s_imgWidth = 135;
    static const uint8_t s_diverSpriteRadius = 15;
    static const uint8_t s_featureSpriteRadius = 5;
    static const uint16_t s_diverSpriteColour;
    static const uint16_t s_featureSpriteColour;
    static const bool     s_useSpriteForFeatures = true;

    double _lastDiverLatitude;
    double _lastDiverLongitude;
    double _lastDiverHeading;

    bool _useDiverHeading;
    
    const geo_map *_currentMap, *_previousMap;

    bool _showAllLake;

    const geo_map* _northMap=s_maps;          const uint8_t _northMapIndex = 0;
    const geo_map* _cafeJettyMap=s_maps+1;   const uint8_t _cafeJettyMapIndex = 1;
    const geo_map* _midJettyMap=s_maps+2;    const uint8_t _midJettyMapIndex = 2;
    const geo_map* _southMap=s_maps+3;       const uint8_t _southMapIndex = 3;
    const geo_map* _allLakeMap=s_maps+4;     const uint8_t _allLakeMapIndex = 4;
    const geo_map* _canoeZoneMap=s_maps+5;   const uint8_t _canoeZoneMapIndex = 5;
    const geo_map* _subZoneMap=s_maps+6;     const uint8_t _subZoneMapIndex = 6;

    const navigationWaypoint* _targetWaypoint;
    const navigationWaypoint* _prevWaypoint;
    const navigationWaypoint* _closestExitWaypoint;
    std::string               _prevTargetLabel;
   
    int16_t _zoom;
    int16_t _priorToZoneZoom;
    int16_t _tileXToDisplay;
    int16_t _tileYToDisplay;

    bool _drawAllFeatures;
    
    void initSprites();
    void initFeatureToMapsLookup();
    void initExitWaypoints();
    void initMapsForFeature(const navigationWaypoint* waypoint, geoRef& ref);

    void drawFeaturesOnCleanMapSprite(const geo_map* featureMap);
//    void drawFeaturesOnCleanMapSprite(const geo_map* featureMap,uint8_t zoom, uint8_t tileX, uint8_t tileY);
    pixel convertGeoToPixelDouble(double latitude, double longitude, const geo_map* mapToPlot) const;
    pixel convertGeoToPixelFloat(float latitude, float longitude, const geo_map* mapToPlot) const;
    
    const geo_map* getNextMapByPixelLocation(const MapScreen::pixel loc, const geo_map* thisMap);
    MapScreen::pixel scalePixelForZoomedInTile(const pixel p, int16_t& tileX, int16_t& tileY) const;

    bool isPixelInCanoeZone(const MapScreen::pixel loc, const geo_map* thisMap) const;
    bool isPixelInSubZone(const MapScreen::pixel loc, const geo_map* thisMap) const;
    bool isPixelOutsideScreenExtent(const MapScreen::pixel loc) const;

    void debugPixelMapOutput(const MapScreen::pixel loc, const geo_map* thisMap, const geo_map* nextMap) const;
    void debugPixelFeatureOutput(navigationWaypoint* waypoint, MapScreen::pixel loc, const geo_map* thisMap) const;
    void debugScaledPixelForTile(pixel p, pixel pScaled, int16_t tileX,int16_t tileY) const;
};

#endif
