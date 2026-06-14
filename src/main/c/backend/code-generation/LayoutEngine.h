#ifndef LAYOUT_ENGINE_HEADER
#define LAYOUT_ENGINE_HEADER

#include "../../frontend/syntactic-analysis/AbstractSyntaxTree.h"
#include <stdbool.h>

typedef struct {
    Body *body;
    double x;
    double y;
    double width;
    double height;
    double rotation;
    double subtreeSpan;
    double massLabelAngle;
    int parentIndex;
} BodyLayout;

typedef struct {
    Surface *surface;
    double originX;
    double originY;
    double tangentX;
    double tangentY;
    double normalX;
    double normalY;
    double angleDegrees;
} SurfaceFrame;

typedef struct {
    double minX;
    double maxX;
    double minY;
    double maxY;
} DiagramBounds;

/* Geometry helpers */
double _degreesToRadians(double degrees);
double _angleToDegrees(Value angle, AngleUnit unit);
double _distanceUnitInMeters(DistanceUnit unit);
double _distanceInMeters(Value value, DistanceUnit unit);
double _polygonDisplayScale(Surface *surface);
double _polygonDisplayX(Surface *surface, Point *point);
double _polygonDisplayY(Surface *surface, Point *point);
DistanceUnit _dimensionUnit(DistanceUnit firstUnit, DistanceUnit secondUnit, double lengthMeters);
DistanceUnit _coordinateDeltaUnit(Value origin, DistanceUnit originUnit, Value endpoint,
                                  DistanceUnit endpointUnit, double lengthMeters);

/* Angle helpers */
double _normalizeAngle(double degrees);
double _readableBodyRotation(double degrees);
bool _anglesArePerpendicular(double firstDegrees, double secondDegrees);
int _directionBucket(double degrees);
const char *_labelAnchorForAngle(double degrees);
const char *_labelAnchorForVector(double x, double y);

/* Body counting and lookup */
int _countBodies(System *system);
int _findBodyLayout(BodyLayout *layouts, int count, const char *name);
char *_bodySubscript(Body *body);

/* Body dimensions and geometry */
void _bodyDimensions(Body *body, double *width, double *height);
double _bodyExtentAlong(const BodyLayout *layout, double directionX, double directionY);
double _bodyRayExtent(const BodyLayout *layout, double angleDegrees);
double _rayIntersectionDistance(const BodyLayout *layout, double originX, double originY,
                                double directionX, double directionY);
double _availableForceLength(const BodyLayout *source, BodyLayout *layouts, int count,
                             double startX, double startY, double directionX, double directionY);

/* Surface frame */
SurfaceFrame _surfaceFrame(System *system);
void _supportRange(const SurfaceFrame *frame, BodyLayout *layouts, int count,
                   double *minimum, double *maximum);

/* Layout computation */
double _computeSubtreeSpan(BodyLayout *layouts, int count, int index);
void _positionChildren(BodyLayout *layouts, int count, int parentIndex, const SurfaceFrame *frame);
BodyLayout *_computeBodyLayouts(System *system, int count, SurfaceFrame *frame);
void _shiftBodySubtree(BodyLayout *layouts, int count, int index, double deltaX, double deltaY);
void _applyPolarDistances(System *system, BodyLayout *layouts, int count);
DiagramBounds _diagramBounds(System *system, BodyLayout *layouts, int count);
void _expandForceCorridors(System *system, BodyLayout *layouts, int count, const SurfaceFrame *frame);

/* Force layout helpers */
double _resolveForceAngle(Force *force, const SurfaceFrame *frame);
bool _bodyHasExplicitForceAlong(Body *body, const SurfaceFrame *frame, double angleDegrees);
double _implicitForceAngle(ImplicitForceType type, const SurfaceFrame *frame);
double _laneOffset(int lane);
bool _isAxisAligned(double angleDegrees);
double _massLabelAngle(Body *body, const SurfaceFrame *frame, double preferredAngle);
void _assignMassLabelAngles(BodyLayout *layouts, int count, const SurfaceFrame *frame);

#endif
