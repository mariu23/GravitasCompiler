#include "LayoutEngine.h"
#include "LatexUtils.h"

#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

static const double _pi = 3.14159265358979323846;
static const double _bodyGap = 1.4;
static const double _surfaceClearance = 0.08;
static const double _surfaceY = -1.5;
static const double _arrowLength = 2.2;

/* Geometry helpers */

double _degreesToRadians(double degrees) {
    return degrees * _pi / 180.0;
}

double _angleToDegrees(Value angle, AngleUnit unit) {
    if (unit == ANGLE_UNIT_RADIAN) { return angle.numericValue * 180.0 / _pi; }
    return angle.numericValue;
}

double _distanceUnitInMeters(DistanceUnit unit) {
    switch (unit) {
        case DISTANCE_UNIT_CENTIMETER:
            return 0.01;
        case DISTANCE_UNIT_MILLIMETER:
            return 0.001;
        case DISTANCE_UNIT_KILOMETER:
            return 1000.0;
        case DISTANCE_UNIT_METER:
        case DISTANCE_UNIT_DEFAULT:
            return 1.0;
    }
    return 1.0;
}

double _distanceInMeters(Value value, DistanceUnit unit) {
    return value.numericValue * _distanceUnitInMeters(unit);
}

double _polygonDisplayScale(Surface *surface) {
    double minX = DBL_MAX;
    double maxX = -DBL_MAX;
    double minY = DBL_MAX;
    double maxY = -DBL_MAX;
    for (AstList *vertex = surface->vertices; vertex != NULL; vertex = vertex->next) {
        Point *point = (Point *) vertex->value;
        double x = _distanceInMeters(point->x, point->xUnit);
        double y = _distanceInMeters(point->y, point->yUnit);
        minX = fmin(minX, x);
        maxX = fmax(maxX, x);
        minY = fmin(minY, y);
        maxY = fmax(maxY, y);
    }
    double maximumSpan = fmax(maxX - minX, maxY - minY);
    return maximumSpan > 8.0 ? 8.0 / maximumSpan : 1.0;
}

double _polygonDisplayX(Surface *surface, Point *point) {
    return _distanceInMeters(point->x, point->xUnit) * _polygonDisplayScale(surface);
}

double _polygonDisplayY(Surface *surface, Point *point) {
    return _distanceInMeters(point->y, point->yUnit) * _polygonDisplayScale(surface);
}

DistanceUnit _dimensionUnit(DistanceUnit firstUnit, DistanceUnit secondUnit, double lengthMeters) {
    if (firstUnit == secondUnit) { return firstUnit; }
    if (lengthMeters >= 1000.0) { return DISTANCE_UNIT_KILOMETER; }
    if (lengthMeters >= 1.0) { return DISTANCE_UNIT_METER; }
    if (lengthMeters >= 0.01) { return DISTANCE_UNIT_CENTIMETER; }
    return DISTANCE_UNIT_MILLIMETER;
}

DistanceUnit _coordinateDeltaUnit(Value origin, DistanceUnit originUnit, Value endpoint,
                                  DistanceUnit endpointUnit, double lengthMeters) {
    if (fabs(origin.numericValue) < 1e-9) { return endpointUnit; }
    if (fabs(endpoint.numericValue) < 1e-9) { return originUnit; }
    return _dimensionUnit(originUnit, endpointUnit, lengthMeters);
}

/* Angle helpers */

double _normalizeAngle(double degrees) {
    double normalized = degrees;
    while (normalized >= 360.0) { normalized -= 360.0; }
    while (normalized < 0.0) { normalized += 360.0; }
    return normalized;
}

double _readableBodyRotation(double degrees) {
    double rotation = _normalizeAngle(degrees);
    if (rotation > 180.0) { rotation -= 360.0; }
    if (rotation > 90.0) { rotation -= 180.0; }
    if (rotation <= -90.0) { rotation += 180.0; }
    return rotation;
}

bool _anglesArePerpendicular(double firstDegrees, double secondDegrees) {
    double difference = _normalizeAngle(firstDegrees - secondDegrees);
    return fabs(difference - 90.0) < 1e-6 || fabs(difference - 270.0) < 1e-6;
}

int _directionBucket(double degrees) {
    return ((int) ((_normalizeAngle(degrees) + 22.5) / 45.0)) % 8;
}

const char *_labelAnchorForAngle(double degrees) {
    static const char *anchors[] = {"west", "south west", "south", "south east",
                                    "east", "north east", "north", "north west"};
    return anchors[_directionBucket(degrees)];
}

const char *_labelAnchorForVector(double x, double y) {
    if (fabs(x) > 2.0 * fabs(y)) { return x >= 0.0 ? "west" : "east"; }
    if (fabs(y) > 2.0 * fabs(x)) { return y >= 0.0 ? "south" : "north"; }
    if (x >= 0.0 && y >= 0.0) { return "south west"; }
    if (x < 0.0 && y >= 0.0) { return "south east"; }
    if (x < 0.0 && y < 0.0) { return "north east"; }
    return "north west";
}

/* Body counting and lookup */

int _countBodies(System *system) {
    int count = 0;
    for (AstList *item = system->bodies; item != NULL; item = item->next) { count++; }
    return count;
}

int _findBodyLayout(BodyLayout *layouts, int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(layouts[i].body->name, name) == 0) { return i; }
    }
    return -1;
}

char *_bodySubscript(Body *body) {
    if (body == NULL || body->name == NULL || body->name[0] == '\0') { return NULL; }
    char initial[] = {body->name[0], '\0'};
    return _escapeLatex(initial);
}

void _bodyDimensions(Body *body, double *width, double *height) {
    size_t nameLength = body->name == NULL ? 0 : strlen(body->name);
    double labelWidth = 0.22 * nameLength + 0.55;
    if (body->shape == BODY_SHAPE_SPHERE) {
        *width = fmax(1.2, labelWidth);
        *height = *width;
    } else {
        *width = fmax(1.4, labelWidth);
        *height = 0.9;
    }
}

double _bodyExtentAlong(const BodyLayout *layout, double directionX, double directionY) {
    if (layout->body->shape == BODY_SHAPE_SPHERE) { return layout->width / 2.0; }

    double rotation = _degreesToRadians(layout->rotation);
    double localX = directionX * cos(rotation) + directionY * sin(rotation);
    double localY = -directionX * sin(rotation) + directionY * cos(rotation);
    return fabs(localX) * layout->width / 2.0 + fabs(localY) * layout->height / 2.0;
}

double _bodyRayExtent(const BodyLayout *layout, double angleDegrees) {
    if (layout->body->shape == BODY_SHAPE_SPHERE) { return layout->width / 2.0; }

    double localAngle = _degreesToRadians(angleDegrees - layout->rotation);
    double x = fabs(cos(localAngle));
    double y = fabs(sin(localAngle));
    double xExtent = x < 1e-9 ? DBL_MAX : layout->width / (2.0 * x);
    double yExtent = y < 1e-9 ? DBL_MAX : layout->height / (2.0 * y);
    return fmin(xExtent, yExtent);
}

double _rayIntersectionDistance(const BodyLayout *layout, double originX, double originY, double directionX,
                                double directionY) {
    double relativeX = originX - layout->x;
    double relativeY = originY - layout->y;
    if (layout->body->shape == BODY_SHAPE_SPHERE) {
        double radius = layout->width / 2.0;
        double projection = relativeX * directionX + relativeY * directionY;
        double discriminant =
            projection * projection - (relativeX * relativeX + relativeY * relativeY - radius * radius);
        if (discriminant < 0.0) { return DBL_MAX; }
        double distance = -projection - sqrt(discriminant);
        return distance > 1e-6 ? distance : DBL_MAX;
    }

    double rotation = _degreesToRadians(layout->rotation);
    double localOriginX = relativeX * cos(rotation) + relativeY * sin(rotation);
    double localOriginY = -relativeX * sin(rotation) + relativeY * cos(rotation);
    double localDirectionX = directionX * cos(rotation) + directionY * sin(rotation);
    double localDirectionY = -directionX * sin(rotation) + directionY * cos(rotation);
    double minimumDistance = -DBL_MAX;
    double maximumDistance = DBL_MAX;
    double origins[] = {localOriginX, localOriginY};
    double directions[] = {localDirectionX, localDirectionY};
    double extents[] = {layout->width / 2.0, layout->height / 2.0};

    for (int axis = 0; axis < 2; axis++) {
        if (fabs(directions[axis]) < 1e-9) {
            if (fabs(origins[axis]) > extents[axis]) { return DBL_MAX; }
            continue;
        }
        double first = (-extents[axis] - origins[axis]) / directions[axis];
        double second = (extents[axis] - origins[axis]) / directions[axis];
        if (first > second) {
            double swap = first;
            first = second;
            second = swap;
        }
        minimumDistance = fmax(minimumDistance, first);
        maximumDistance = fmin(maximumDistance, second);
        if (minimumDistance > maximumDistance) { return DBL_MAX; }
    }
    return minimumDistance > 1e-6 ? minimumDistance : DBL_MAX;
}

double _availableForceLength(const BodyLayout *source, BodyLayout *layouts, int count, double startX,
                             double startY, double directionX, double directionY) {
    double length = _arrowLength;
    for (int i = 0; i < count; i++) {
        if (&layouts[i] == source) { continue; }
        double intersection = _rayIntersectionDistance(&layouts[i], startX, startY, directionX, directionY);
        if (intersection != DBL_MAX) { length = fmin(length, fmax(0.08, intersection - 0.12)); }
    }
    return length;
}

/* Surface frame */

SurfaceFrame _surfaceFrame(System *system) {
    SurfaceFrame frame = {.surface = NULL,
                          .originX = 0.0,
                          .originY = _surfaceY,
                          .tangentX = 1.0,
                          .tangentY = 0.0,
                          .normalX = 0.0,
                          .normalY = 1.0,
                          .angleDegrees = 0.0};
    if (system->surfaces == NULL) {
        frame.originY = 0.0;
        return frame;
    }

    frame.surface = (Surface *) system->surfaces->value;
    if (frame.surface->type == SURFACE_TYPE_POLYGON && frame.surface->vertices != NULL &&
        frame.surface->vertices->next != NULL) {
        Point *first = (Point *) frame.surface->vertices->value;
        Point *second = (Point *) frame.surface->vertices->next->value;
        double firstX = _polygonDisplayX(frame.surface, first);
        double firstY = _polygonDisplayY(frame.surface, first);
        double secondX = _polygonDisplayX(frame.surface, second);
        double secondY = _polygonDisplayY(frame.surface, second);
        double deltaX = secondX - firstX;
        double deltaY = secondY - firstY;
        double length = sqrt(deltaX * deltaX + deltaY * deltaY);
        if (length > 1e-9) {
            frame.originX = (firstX + secondX) / 2.0;
            frame.originY = (firstY + secondY) / 2.0;
            frame.tangentX = deltaX / length;
            frame.tangentY = deltaY / length;
            frame.angleDegrees = atan2(frame.tangentY, frame.tangentX) * 180.0 / _pi;

            double signedArea = 0.0;
            AstList *firstNode = frame.surface->vertices;
            for (AstList *vertex = firstNode; vertex != NULL; vertex = vertex->next) {
                AstList *nextVertex = vertex->next != NULL ? vertex->next : firstNode;
                Point *a = (Point *) vertex->value;
                Point *b = (Point *) nextVertex->value;
                signedArea += _polygonDisplayX(frame.surface, a) * _polygonDisplayY(frame.surface, b) -
                              _polygonDisplayX(frame.surface, b) * _polygonDisplayY(frame.surface, a);
            }
            if (signedArea >= 0.0) {
                frame.normalX = frame.tangentY;
                frame.normalY = -frame.tangentX;
            } else {
                frame.normalX = -frame.tangentY;
                frame.normalY = frame.tangentX;
            }
        }
    } else if (frame.surface->type == SURFACE_TYPE_INCLINE && frame.surface->hasAngle) {
        frame.angleDegrees = _angleToDegrees(frame.surface->angle, frame.surface->angleUnit);
        double radians = _degreesToRadians(frame.angleDegrees);
        frame.tangentX = cos(radians);
        frame.tangentY = sin(radians);
        frame.normalX = -sin(radians);
        frame.normalY = cos(radians);
    }
    return frame;
}

void _supportRange(const SurfaceFrame *frame, BodyLayout *layouts, int count, double *minimum, double *maximum) {
    *minimum = -3.0;
    *maximum = 3.0;
    bool foundBody = false;
    for (int i = 0; i < count; i++) {
        if (layouts[i].parentIndex != -1) { continue; }
        double relativeX = layouts[i].x - frame->originX;
        double relativeY = layouts[i].y - frame->originY;
        double projection = relativeX * frame->tangentX + relativeY * frame->tangentY;
        double extent = _bodyExtentAlong(&layouts[i], frame->tangentX, frame->tangentY) + 1.4;
        if (!foundBody) {
            *minimum = projection - extent;
            *maximum = projection + extent;
            foundBody = true;
        } else {
            *minimum = fmin(*minimum, projection - extent);
            *maximum = fmax(*maximum, projection + extent);
        }
    }
}

/* Layout computation */

double _computeSubtreeSpan(BodyLayout *layouts, int count, int index) {
    if (layouts[index].subtreeSpan > 0.0) { return layouts[index].subtreeSpan; }

    double childrenSpan = 0.0;
    int childCount = 0;
    for (int i = 0; i < count; i++) {
        if (layouts[i].parentIndex != index) { continue; }
        if (childCount > 0) { childrenSpan += _bodyGap; }
        childrenSpan += _computeSubtreeSpan(layouts, count, i);
        childCount++;
    }

    layouts[index].subtreeSpan = fmax(layouts[index].width, childrenSpan);
    return layouts[index].subtreeSpan;
}

void _positionChildren(BodyLayout *layouts, int count, int parentIndex, const SurfaceFrame *frame) {
    double childrenSpan = 0.0;
    int childCount = 0;
    for (int i = 0; i < count; i++) {
        if (layouts[i].parentIndex != parentIndex) { continue; }
        if (childCount > 0) { childrenSpan += _bodyGap; }
        childrenSpan += layouts[i].subtreeSpan;
        childCount++;
    }

    double cursor = -childrenSpan / 2.0;
    for (int i = 0; i < count; i++) {
        if (layouts[i].parentIndex != parentIndex) { continue; }
        double tangentOffset = cursor + layouts[i].subtreeSpan / 2.0;
        double parentExtent = _bodyExtentAlong(&layouts[parentIndex], frame->normalX, frame->normalY);
        double childExtent = _bodyExtentAlong(&layouts[i], frame->normalX, frame->normalY);
        double normalOffset = parentExtent + childExtent;
        layouts[i].x = layouts[parentIndex].x + tangentOffset * frame->tangentX + normalOffset * frame->normalX;
        layouts[i].y = layouts[parentIndex].y + tangentOffset * frame->tangentY + normalOffset * frame->normalY;
        cursor += layouts[i].subtreeSpan + _bodyGap;
        _positionChildren(layouts, count, i, frame);
    }
}

BodyLayout *_computeBodyLayouts(System *system, int count, SurfaceFrame *frame) {
    if (count == 0) { return NULL; }

    BodyLayout *layouts = calloc(count, sizeof(BodyLayout));
    int index = 0;
    for (AstList *item = system->bodies; item != NULL; item = item->next, index++) {
        layouts[index].body = (Body *) item->value;
        layouts[index].parentIndex = -1;
        _bodyDimensions(layouts[index].body, &layouts[index].width, &layouts[index].height);
        if (frame->surface != NULL &&
            (frame->surface->type == SURFACE_TYPE_INCLINE || frame->surface->type == SURFACE_TYPE_POLYGON) &&
            layouts[index].body->shape != BODY_SHAPE_SPHERE) {
            layouts[index].rotation = _readableBodyRotation(frame->angleDegrees);
        }
    }

    for (int i = 0; i < count; i++) {
        if (layouts[i].body->parentBodyName != NULL) {
            layouts[i].parentIndex = _findBodyLayout(layouts, count, layouts[i].body->parentBodyName);
        }
    }

    double rootsSpan = 0.0;
    int rootCount = 0;
    for (int i = 0; i < count; i++) {
        if (layouts[i].parentIndex != -1) { continue; }
        if (rootCount > 0) { rootsSpan += _bodyGap; }
        rootsSpan += _computeSubtreeSpan(layouts, count, i);
        rootCount++;
    }

    double cursor = -rootsSpan / 2.0;
    for (int i = 0; i < count; i++) {
        if (layouts[i].parentIndex != -1) { continue; }
        double tangentPosition = cursor + layouts[i].subtreeSpan / 2.0;
        double supportX = frame->originX + tangentPosition * frame->tangentX;
        double supportY = frame->originY + tangentPosition * frame->tangentY;

        if (frame->surface == NULL) {
            layouts[i].x = tangentPosition;
            layouts[i].y = 0.0;
        } else {
            double normalExtent = _bodyExtentAlong(&layouts[i], frame->normalX, frame->normalY);
            layouts[i].x = supportX + (normalExtent + _surfaceClearance) * frame->normalX;
            layouts[i].y = supportY + (normalExtent + _surfaceClearance) * frame->normalY;
        }

        cursor += layouts[i].subtreeSpan + _bodyGap;
        _positionChildren(layouts, count, i, frame);
    }
    return layouts;
}

void _shiftBodySubtree(BodyLayout *layouts, int count, int index, double deltaX, double deltaY) {
    layouts[index].x += deltaX;
    layouts[index].y += deltaY;
    for (int i = 0; i < count; i++) {
        if (layouts[i].parentIndex == index) { _shiftBodySubtree(layouts, count, i, deltaX, deltaY); }
    }
}

void _applyPolarDistances(System *system, BodyLayout *layouts, int count) {
    for (AstList *distanceNode = system->distances; distanceNode != NULL; distanceNode = distanceNode->next) {
        Distance *distance = (Distance *) distanceNode->value;
        if (distance->type != DISTANCE_TYPE_POLAR) { continue; }

        int fromIndex = _findBodyLayout(layouts, count, distance->fromBodyName);
        int toIndex = _findBodyLayout(layouts, count, distance->toBodyName);
        if (fromIndex < 0 || toIndex < 0 || fromIndex == toIndex) { continue; }

        double angleDegrees = _angleToDegrees(distance->polar.angle, distance->polar.angleUnit);
        double radians = _degreesToRadians(angleDegrees);
        double magnitude = fabs(distance->polar.magnitude.numericValue);
        double targetX = layouts[fromIndex].x + magnitude * cos(radians);
        double targetY = layouts[fromIndex].y + magnitude * sin(radians);
        _shiftBodySubtree(layouts, count, toIndex, targetX - layouts[toIndex].x, targetY - layouts[toIndex].y);
    }
}

DiagramBounds _diagramBounds(System *system, BodyLayout *layouts, int count) {
    DiagramBounds bounds = {.minX = DBL_MAX, .maxX = -DBL_MAX, .minY = DBL_MAX, .maxY = -DBL_MAX};

    for (int i = 0; i < count; i++) {
        double extentX = _bodyExtentAlong(&layouts[i], 1.0, 0.0) + _arrowLength + 0.8;
        double extentY = _bodyExtentAlong(&layouts[i], 0.0, 1.0) + _arrowLength + 0.8;
        bounds.minX = fmin(bounds.minX, layouts[i].x - extentX);
        bounds.maxX = fmax(bounds.maxX, layouts[i].x + extentX);
        bounds.minY = fmin(bounds.minY, layouts[i].y - extentY);
        bounds.maxY = fmax(bounds.maxY, layouts[i].y + extentY);
    }

    for (AstList *surfaceNode = system->surfaces; surfaceNode != NULL; surfaceNode = surfaceNode->next) {
        Surface *surface = (Surface *) surfaceNode->value;
        if (surface->type != SURFACE_TYPE_POLYGON) { continue; }
        for (AstList *vertex = surface->vertices; vertex != NULL; vertex = vertex->next) {
            Point *point = (Point *) vertex->value;
            double x = _polygonDisplayX(surface, point);
            double y = _polygonDisplayY(surface, point);
            bounds.minX = fmin(bounds.minX, x);
            bounds.maxX = fmax(bounds.maxX, x);
            bounds.minY = fmin(bounds.minY, y);
            bounds.maxY = fmax(bounds.maxY, y);
        }
    }

    if (bounds.minX == DBL_MAX) { return (DiagramBounds){.minX = -3.0, .maxX = 3.0, .minY = -2.0, .maxY = 2.0}; }
    return bounds;
}

void _expandForceCorridors(System *system, BodyLayout *layouts, int count, const SurfaceFrame *frame) {
    const double minimumGap = 1.55;
    for (AstList *distanceNode = system->distances; distanceNode != NULL; distanceNode = distanceNode->next) {
        Distance *distance = (Distance *) distanceNode->value;
        if (distance->type != DISTANCE_TYPE_POLAR) { continue; }

        int fromIndex = _findBodyLayout(layouts, count, distance->fromBodyName);
        int toIndex = _findBodyLayout(layouts, count, distance->toBodyName);
        if (fromIndex < 0 || toIndex < 0 || fromIndex == toIndex) { continue; }

        double angleDegrees = _angleToDegrees(distance->polar.angle, distance->polar.angleUnit);
        if (!_bodyHasExplicitForceAlong(layouts[fromIndex].body, frame, angleDegrees) &&
            !_bodyHasExplicitForceAlong(layouts[toIndex].body, frame, angleDegrees + 180.0)) {
            continue;
        }

        double radians = _degreesToRadians(angleDegrees);
        double directionX = cos(radians);
        double directionY = sin(radians);
        double centerDistance = (layouts[toIndex].x - layouts[fromIndex].x) * directionX +
                                (layouts[toIndex].y - layouts[fromIndex].y) * directionY;
        double gap = centerDistance - _bodyExtentAlong(&layouts[fromIndex], directionX, directionY) -
                     _bodyExtentAlong(&layouts[toIndex], -directionX, -directionY);
        if (gap < minimumGap) {
            double expansion = minimumGap - gap;
            _shiftBodySubtree(layouts, count, toIndex, expansion * directionX, expansion * directionY);
        }
    }
}

/* Force layout helpers */

double _resolveForceAngle(Force *force, const SurfaceFrame *frame) {
    if (force->direction->type == DIRECTION_TYPE_ABSOLUTE_ANGLE) {
        return frame->angleDegrees + _angleToDegrees(force->direction->angle, force->direction->angleUnit);
    }
    return frame->angleDegrees;
}

bool _bodyHasExplicitForceAlong(Body *body, const SurfaceFrame *frame, double angleDegrees) {
    for (AstList *forceNode = body->forces; forceNode != NULL; forceNode = forceNode->next) {
        double difference = _normalizeAngle(_resolveForceAngle((Force *) forceNode->value, frame) - angleDegrees);
        if (difference < 1e-6 || 360.0 - difference < 1e-6) { return true; }
    }
    return false;
}

double _implicitForceAngle(ImplicitForceType type, const SurfaceFrame *frame) {
    switch (type) {
        case IMPLICIT_FORCE_WEIGHT:
            return -90.0;
        case IMPLICIT_FORCE_NORMAL:
            return frame->angleDegrees + 90.0;
        case IMPLICIT_FORCE_FRICTION:
            return frame->angleDegrees;
    }
    return 0.0;
}

double _laneOffset(int lane) {
    if (lane == 0) { return 0.0; }
    double magnitude = ((lane + 1) / 2) * 0.32;
    return lane % 2 == 1 ? magnitude : -magnitude;
}

bool _isAxisAligned(double angleDegrees) {
    double normalized = _normalizeAngle(angleDegrees);
    double remainder = normalized;
    while (remainder >= 90.0) { remainder -= 90.0; }
    return remainder < 1e-6 || 90.0 - remainder < 1e-6;
}

double _massLabelAngle(Body *body, const SurfaceFrame *frame, double preferredAngle) {
    bool occupied[8] = {false};
    for (AstList *forceNode = body->forces; forceNode != NULL; forceNode = forceNode->next) {
        occupied[_directionBucket(_resolveForceAngle((Force *) forceNode->value, frame))] = true;
    }
    if (body->implicitForces != NULL) {
        for (AstList *forceNode = body->implicitForces->forces; forceNode != NULL; forceNode = forceNode->next) {
            ImplicitForce *force = (ImplicitForce *) forceNode->value;
            occupied[_directionBucket(_implicitForceAngle(force->type, frame))] = true;
        }
    }

    double preferredAngles[] = {preferredAngle,         preferredAngle + 45.0, preferredAngle - 45.0,
                                preferredAngle + 90.0,  preferredAngle - 90.0, preferredAngle + 135.0,
                                preferredAngle - 135.0, preferredAngle + 180.0};
    for (size_t i = 0; i < sizeof(preferredAngles) / sizeof(preferredAngles[0]); i++) {
        if (!occupied[_directionBucket(preferredAngles[i])]) { return preferredAngles[i]; }
    }
    return preferredAngle + 45.0;
}

void _assignMassLabelAngles(BodyLayout *layouts, int count, const SurfaceFrame *frame) {
    if (count == 0) { return; }
    double minimum = DBL_MAX;
    double maximum = -DBL_MAX;
    for (int i = 0; i < count; i++) {
        double projection = layouts[i].x * frame->tangentX + layouts[i].y * frame->tangentY;
        minimum = fmin(minimum, projection);
        maximum = fmax(maximum, projection);
    }

    double center = (minimum + maximum) / 2.0;
    for (int i = 0; i < count; i++) {
        double projection = layouts[i].x * frame->tangentX + layouts[i].y * frame->tangentY;
        double preferredAngle = frame->angleDegrees + 180.0;
        if (projection > center + 0.25) { preferredAngle = frame->angleDegrees; }
        layouts[i].massLabelAngle = _massLabelAngle(layouts[i].body, frame, preferredAngle);
    }
}
