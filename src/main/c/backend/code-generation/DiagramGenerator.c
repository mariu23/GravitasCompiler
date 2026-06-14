#include "DiagramGenerator.h"
#include "LatexUtils.h"
#include "LayoutEngine.h"

#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* MODULE INTERNAL STATE */

static Logger *_logger = NULL;
static const double _pi = 3.14159265358979323846;
static const double _surfaceY = -1.5;
static const double _arrowLength = 2.2;

void _shutdownDiagramGeneratorModule() {
    if (_logger != NULL) {
        logDebugging(_logger, "Destroying module: DiagramGenerator...");
        destroyLogger(_logger);
        _logger = NULL;
    }
}

ModuleDestructor initializeDiagramGeneratorModule() {
    _logger = createLogger("DiagramGenerator");
    return _shutdownDiagramGeneratorModule;
}

/* PRIVATE FUNCTIONS */

static void _generatePrologue(void);
static void _generateEpilogue(void);
static void _generateSystem(System *system);
static void _generateSurfaces(System *system, BodyLayout *layouts, int count);
static void _generateBodies(System *system, BodyLayout *layouts, int count, const SurfaceFrame *frame);
static void _generateDistances(System *system, BodyLayout *layouts, int count);
static void _generateReferenceFrame(ReferenceFrame *frame, BodyLayout *layouts, int count,
                                    const SurfaceFrame *surfaceFrame);
static void _output(const char *const format, ...);
static void _generateRightAngleMarker(double originX, double originY, double firstDirectionX, double firstDirectionY,
                                      double secondDirectionX, double secondDirectionY);

/* Output */

static void _output(const char *const format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vfprintf(stdout, format, arguments);
    fflush(stdout);
    va_end(arguments);
}

static void _generateRightAngleMarker(double originX, double originY, double firstDirectionX, double firstDirectionY,
                                      double secondDirectionX, double secondDirectionY) {
    const double size = 0.18;
    double firstX = originX + size * firstDirectionX;
    double firstY = originY + size * firstDirectionY;
    double cornerX = firstX + size * secondDirectionX;
    double cornerY = firstY + size * secondDirectionY;
    double secondX = originX + size * secondDirectionX;
    double secondY = originY + size * secondDirectionY;
    _output("    \\draw[thin] (%f, %f) -- (%f, %f) -- (%f, %f);\n", firstX, firstY, cornerX, cornerY, secondX, secondY);
}

/* Polygon dimensions */

static void _generatePolygonDimensions(Surface *surface, double offsetX, double offsetY) {
    Point *origin = (Point *) surface->vertices->value;
    double originXMeters = _distanceInMeters(origin->x, origin->xUnit);
    double originYMeters = _distanceInMeters(origin->y, origin->yUnit);
    double minXMeters = DBL_MAX;
    double maxXMeters = -DBL_MAX;
    double minYMeters = DBL_MAX;
    double maxYMeters = -DBL_MAX;
    Point *minXPoint = origin;
    Point *maxXPoint = origin;
    Point *minYPoint = origin;
    Point *maxYPoint = origin;
    for (AstList *vertex = surface->vertices; vertex != NULL; vertex = vertex->next) {
        Point *point = (Point *) vertex->value;
        double xMeters = _distanceInMeters(point->x, point->xUnit);
        double yMeters = _distanceInMeters(point->y, point->yUnit);
        if (xMeters < minXMeters) {
            minXMeters = xMeters;
            minXPoint = point;
        }
        if (xMeters > maxXMeters) {
            maxXMeters = xMeters;
            maxXPoint = point;
        }
        if (yMeters < minYMeters) {
            minYMeters = yMeters;
            minYPoint = point;
        }
        if (yMeters > maxYMeters) {
            maxYMeters = yMeters;
            maxYPoint = point;
        }
    }
    double negativeHorizontalMeters = minXMeters - originXMeters;
    double positiveHorizontalMeters = maxXMeters - originXMeters;
    double negativeVerticalMeters = minYMeters - originYMeters;
    double positiveVerticalMeters = maxYMeters - originYMeters;
    if (fabs(negativeHorizontalMeters) < 1e-9 && fabs(positiveHorizontalMeters) < 1e-9 &&
        fabs(negativeVerticalMeters) < 1e-9 && fabs(positiveVerticalMeters) < 1e-9) {
        return;
    }

    double scale = _polygonDisplayScale(surface);
    double originX = originXMeters * scale + offsetX;
    double originY = originYMeters * scale + offsetY;
    double minX = minXMeters * scale + offsetX;
    double maxX = maxXMeters * scale + offsetX;
    double minY = minYMeters * scale + offsetY;
    double maxY = maxYMeters * scale + offsetY;
    Point *second = (Point *) surface->vertices->next->value;
    double edgeDX = _distanceInMeters(second->x, second->xUnit) - originXMeters;
    double signedArea = 0.0;
    for (AstList *vertex = surface->vertices; vertex != NULL; vertex = vertex->next) {
        AstList *next = vertex->next != NULL ? vertex->next : surface->vertices;
        Point *firstPoint = (Point *) vertex->value;
        Point *secondPoint = (Point *) next->value;
        signedArea +=
            _distanceInMeters(firstPoint->x, firstPoint->xUnit) *
                _distanceInMeters(secondPoint->y, secondPoint->yUnit) -
            _distanceInMeters(secondPoint->x, secondPoint->xUnit) * _distanceInMeters(firstPoint->y, firstPoint->yUnit);
    }
    double outwardNormalY = signedArea >= 0.0 ? -edgeDX : edgeDX;
    bool dimensionsAbove = outwardNormalY < -1e-9;
    double dimensionY = dimensionsAbove ? maxY + 0.42 : minY - 0.42;
    const char *horizontalLabelSide = dimensionsAbove ? "above" : "below";

    _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", originX, originY, originX, dimensionY);
    if (negativeHorizontalMeters < -1e-9) {
        DistanceUnit unit = _coordinateDeltaUnit(origin->x, origin->xUnit, minXPoint->x, minXPoint->xUnit,
                                                 fabs(negativeHorizontalMeters));
        char number[64];
        _formatDecimal(negativeHorizontalMeters / _distanceUnitInMeters(unit), number, sizeof(number));
        char *unitLatex = _distanceUnitToString(unit);
        _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", minX, minY, minX, dimensionY);
        _output("    \\draw[<->,thin] (%f, %f) -- "
                "node[midway,sloped,%s=1pt,inner sep=1pt] {\\scriptsize $%s%s$} (%f, %f);\n",
                originX, dimensionY, horizontalLabelSide, number, unitLatex, minX, dimensionY);
        free(unitLatex);
    }
    if (positiveHorizontalMeters > 1e-9) {
        DistanceUnit unit =
            _coordinateDeltaUnit(origin->x, origin->xUnit, maxXPoint->x, maxXPoint->xUnit, positiveHorizontalMeters);
        char number[64];
        _formatDecimal(positiveHorizontalMeters / _distanceUnitInMeters(unit), number, sizeof(number));
        char *unitLatex = _distanceUnitToString(unit);
        _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", maxX, minY, maxX, dimensionY);
        _output("    \\draw[<->,thin] (%f, %f) -- "
                "node[midway,sloped,%s=1pt,inner sep=1pt] {\\scriptsize $%s%s$} (%f, %f);\n",
                originX, dimensionY, horizontalLabelSide, number, unitLatex, maxX, dimensionY);
        free(unitLatex);
    }
    if (negativeVerticalMeters < -1e-9) {
        DistanceUnit unit = _coordinateDeltaUnit(origin->y, origin->yUnit, minYPoint->y, minYPoint->yUnit,
                                                 fabs(negativeVerticalMeters));
        char number[64];
        _formatDecimal(negativeVerticalMeters / _distanceUnitInMeters(unit), number, sizeof(number));
        char *unitLatex = _distanceUnitToString(unit);
        _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", originX, originY, originX, minY);
        _output("    \\node[anchor=west] at (%f, %f) {\\scriptsize $%s%s$};\n", originX + 0.12, (originY + minY) / 2.0,
                number, unitLatex);
        free(unitLatex);
    }
    if (positiveVerticalMeters > 1e-9) {
        DistanceUnit unit =
            _coordinateDeltaUnit(origin->y, origin->yUnit, maxYPoint->y, maxYPoint->yUnit, positiveVerticalMeters);
        char number[64];
        _formatDecimal(positiveVerticalMeters / _distanceUnitInMeters(unit), number, sizeof(number));
        char *unitLatex = _distanceUnitToString(unit);
        _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", originX, dimensionY, originX, maxY);
        _output("    \\node[anchor=west] at (%f, %f) {\\scriptsize $%s%s$};\n", originX + 0.12,
                originY + 0.55 * positiveVerticalMeters * scale, number, unitLatex);
        free(unitLatex);
    }
    if ((negativeHorizontalMeters < -1e-9 || positiveHorizontalMeters > 1e-9) &&
        (negativeVerticalMeters < -1e-9 || positiveVerticalMeters > 1e-9)) {
        _generateRightAngleMarker(originX, dimensionY, positiveHorizontalMeters > 1e-9 ? 1.0 : -1.0, 0.0, 0.0,
                                  dimensionsAbove ? -1.0 : 1.0);
    }
}

/* Force drawing helpers */

static void _generateForceAngle(double startX, double startY, double referenceAngleDegrees, double relativeAngleDegrees,
                                Value angle, AngleUnit angleUnit) {
    if (_isAxisAligned(relativeAngleDegrees)) { return; }

    double signedAngle = _normalizeAngle(relativeAngleDegrees);
    if (signedAngle > 180.0) { signedAngle -= 360.0; }
    double radius = 0.55;
    double referenceRadians = _degreesToRadians(referenceAngleDegrees);
    double referenceX = startX + radius * cos(referenceRadians);
    double referenceY = startY + radius * sin(referenceRadians);
    double labelAngle = referenceAngleDegrees + signedAngle / 2.0;
    double labelRadians = _degreesToRadians(labelAngle);
    double labelRadius = radius + 0.22;
    char *angleLatex = _sourceTextToLatex(angle.sourceText);
    char *unit = _angleUnitToString(angleUnit);

    _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", startX, startY, referenceX, referenceY);
    _output("    \\draw[thin] (%f, %f) arc[start angle=%f,end angle=%f,radius=%f];\n", referenceX, referenceY,
            referenceAngleDegrees, referenceAngleDegrees + signedAngle, radius);
    _output("    \\node[anchor=%s] at (%f, %f) {\\scriptsize $%s%s$};\n", _labelAnchorForAngle(labelAngle),
            startX + labelRadius * cos(labelRadians), startY + labelRadius * sin(labelRadians), angleLatex, unit);
    free(angleLatex);
    free(unit);
}

static void _generateForceArrow(const BodyLayout *layout, BodyLayout *layouts, int count, double angleDegrees,
                                double bodyAngleDegrees, bool showRightAngleMarker, const char *label, int *buckets,
                                const char *style) {
    int bucket = _directionBucket(angleDegrees);
    int lane = buckets[bucket]++;
    double radians = _degreesToRadians(angleDegrees);
    double directionX = cos(radians);
    double directionY = sin(radians);
    double perpendicularX = -directionY;
    double perpendicularY = directionX;
    double laneOffset = _laneOffset(lane);
    double startDistance = _bodyRayExtent(layout, angleDegrees) + 0.04;
    double startX = layout->x + startDistance * directionX + laneOffset * 0.18 * perpendicularX;
    double startY = layout->y + startDistance * directionY + laneOffset * 0.18 * perpendicularY;
    double arrowLength = _availableForceLength(layout, layouts, count, startX, startY, directionX, directionY);
    double endX = startX + arrowLength * directionX;
    double endY = startY + arrowLength * directionY;
    bool shortened = arrowLength < _arrowLength - 1e-6;
    double labelOutwardOffset = lane * 0.42;
    double labelX = shortened ? startX + (0.5 * arrowLength + labelOutwardOffset) * directionX + 0.28 * perpendicularX
                              : endX + (0.18 + labelOutwardOffset) * directionX + laneOffset * perpendicularX;
    double labelY = shortened ? startY + (0.5 * arrowLength + labelOutwardOffset) * directionY + 0.28 * perpendicularY
                              : endY + (0.18 + labelOutwardOffset) * directionY + laneOffset * perpendicularY;

    _output("    \\draw[->,thick%s] (%f, %f) -- (%f, %f);\n", style, startX, startY, endX, endY);
    _output("    \\node[anchor=%s] at (%f, %f) {%s};\n", shortened ? "south" : _labelAnchorForAngle(angleDegrees),
            labelX, labelY, label);

    if (showRightAngleMarker && _anglesArePerpendicular(angleDegrees, bodyAngleDegrees)) {
        double bodyRadians = _degreesToRadians(bodyAngleDegrees);
        double bodyDirectionX = cos(bodyRadians);
        double bodyDirectionY = sin(bodyRadians);
        _generateRightAngleMarker(startX, startY, bodyDirectionX, bodyDirectionY, directionX, directionY);
    }
}

static void _generateExplicitForces(Body *body, const BodyLayout *layout, BodyLayout *layouts, int count,
                                    const SurfaceFrame *frame, int *buckets) {
    for (AstList *forceNode = body->forces; forceNode != NULL; forceNode = forceNode->next) {
        Force *force = (Force *) forceNode->value;
        char *name = _escapeLatex(force->name);
        char *magnitude = _sourceTextToLatex(force->magnitude.sourceText);
        char *unit = _forceUnitToString(force->unit);
        size_t labelSize = strlen(name) + strlen(magnitude) + strlen(unit) + 16;
        char *label = malloc(labelSize);
        snprintf(label, labelSize, "$%s = %s%s$", name, magnitude, unit);
        double angleDegrees = _resolveForceAngle(force, frame);
        _generateForceArrow(layout, layouts, count, angleDegrees, frame->angleDegrees, true, label, buckets, "");
        if (force->direction->type == DIRECTION_TYPE_ABSOLUTE_ANGLE) {
            double startDistance = _bodyRayExtent(layout, angleDegrees) + 0.04;
            double radians = _degreesToRadians(angleDegrees);
            double startX = layout->x + startDistance * cos(radians);
            double startY = layout->y + startDistance * sin(radians);
            double relativeAngle = _angleToDegrees(force->direction->angle, force->direction->angleUnit);
            _generateForceAngle(startX, startY, frame->angleDegrees, relativeAngle, force->direction->angle,
                                force->direction->angleUnit);
        }
        free(label);
        free(name);
        free(magnitude);
        free(unit);
    }
}

static char *_implicitForceLabel(ImplicitForce *force, Body *body) {
    if (force->name != NULL) {
        char *name = _escapeLatex(force->name);
        char *label = malloc(strlen(name) + 3);
        sprintf(label, "$%s$", name);
        free(name);
        return label;
    }

    char *subscript = _bodySubscript(body);
    char *label = NULL;
    if (subscript != NULL) {
        label = malloc(strlen(subscript) + 12);
        switch (force->type) {
            case IMPLICIT_FORCE_WEIGHT:
                sprintf(label, "$W_{%s}$", subscript);
                break;
            case IMPLICIT_FORCE_NORMAL:
                sprintf(label, "$N_{%s}$", subscript);
                break;
            case IMPLICIT_FORCE_FRICTION:
                sprintf(label, "$F_{f,%s}$", subscript);
                break;
        }
        free(subscript);
        return label;
    }

    switch (force->type) {
        case IMPLICIT_FORCE_WEIGHT:
            return strdup("$W$");
        case IMPLICIT_FORCE_NORMAL:
            return strdup("$N$");
        case IMPLICIT_FORCE_FRICTION:
            return strdup("$F_f$");
    }
    return strdup("");
}

static void _generateImplicitForces(Body *body, const BodyLayout *layout, BodyLayout *layouts, int count,
                                    const SurfaceFrame *frame, int *buckets) {
    if (body->implicitForces == NULL) { return; }
    for (AstList *forceNode = body->implicitForces->forces; forceNode != NULL; forceNode = forceNode->next) {
        ImplicitForce *force = (ImplicitForce *) forceNode->value;
        double angle = _implicitForceAngle(force->type, frame);
        char *label = _implicitForceLabel(force, body);
        _generateForceArrow(layout, layouts, count, angle, frame->angleDegrees, false, label, buckets, "");
        free(label);
    }
}

static void _generateBodyShape(Body *body, const BodyLayout *layout) {
    char *name = _escapeLatex(body->name);
    const char *nameSize = body->name != NULL && strlen(body->name) > 10 ? "\\scriptsize " : "";
    if (body->shape == BODY_SHAPE_SPHERE) {
        _output("    \\draw[thick,fill=gray!20] (%f, %f) circle (%f);\n", layout->x, layout->y, layout->width / 2.0);
        _output("    \\node at (%f, %f) {%s$%s$};\n", layout->x, layout->y, nameSize, name);
    } else {
        _output("    \\begin{scope}[shift={(%f,%f)},rotate=%f,transform shape]\n", layout->x, layout->y,
                layout->rotation);
        _output("      \\draw[thick,fill=gray!20] (%f, %f) rectangle (%f, %f);\n", -layout->width / 2.0,
                -layout->height / 2.0, layout->width / 2.0, layout->height / 2.0);
        _output("      \\node at (0, 0) {%s$%s$};\n", nameSize, name);
        _output("    \\end{scope}\n");
    }
    free(name);
}

static void _generateBodyAnnotations(Body *body, const BodyLayout *layout, BodyLayout *layouts, int count,
                                     const SurfaceFrame *frame) {
    if (body->mass != NULL) {
        char *mass = _sourceTextToLatex(body->mass->value.sourceText);
        char *unit = _massUnitToString(body->mass->unit);
        char *subscript = _bodySubscript(body);
        double angle = layout->massLabelAngle;
        double radians = _degreesToRadians(angle);
        double directionX = cos(radians);
        double directionY = sin(radians);
        double offset = _bodyRayExtent(layout, angle) + 0.45;
        double labelX = layout->x + offset * directionX;
        double labelY = layout->y + offset * directionY;
        if (subscript != NULL) {
            _output("    \\node[anchor=%s] at (%f, %f) {\\scriptsize $m_{%s} = %s%s$};\n", _labelAnchorForAngle(angle),
                    labelX, labelY, subscript, mass, unit);
        } else {
            _output("    \\node[anchor=%s] at (%f, %f) {\\scriptsize $m = %s%s$};\n", _labelAnchorForAngle(angle),
                    labelX, labelY, mass, unit);
        }
        free(subscript);
        free(mass);
        free(unit);
    }

    int buckets[8] = {0};
    _generateExplicitForces(body, layout, layouts, count, frame, buckets);
    _generateImplicitForces(body, layout, layouts, count, frame, buckets);
}

/* Surfaces */

static void _generateSurface(Surface *surface, BodyLayout *layouts, int count, bool hasPreviousEndpoint,
                             double previousEndX, double previousEndY, double *endX, double *endY) {
    if (surface->type == SURFACE_TYPE_POLYGON) {
        if (surface->vertices == NULL) { return; }
        Point *first = (Point *) surface->vertices->value;
        double offsetX = hasPreviousEndpoint ? previousEndX - _polygonDisplayX(surface, first) : 0.0;
        double offsetY = hasPreviousEndpoint ? previousEndY - _polygonDisplayY(surface, first) : 0.0;
        AstList *firstNode = surface->vertices;
        for (AstList *vertex = firstNode; vertex != NULL; vertex = vertex->next) {
            AstList *nextVertex = vertex->next != NULL ? vertex->next : firstNode;
            Point *a = (Point *) vertex->value;
            Point *b = (Point *) nextVertex->value;
            double x0 = _polygonDisplayX(surface, a) + offsetX;
            double y0 = _polygonDisplayY(surface, a) + offsetY;
            double x1 = _polygonDisplayX(surface, b) + offsetX;
            double y1 = _polygonDisplayY(surface, b) + offsetY;
            _output("    \\draw[thick] (%f, %f) -- (%f, %f);\n", x0, y0, x1, y1);
        }
        _generatePolygonDimensions(surface, offsetX, offsetY);
        Point *last = (Point *) surface->vertices->value;
        for (AstList *vertex = surface->vertices; vertex != NULL; vertex = vertex->next) {
            last = (Point *) vertex->value;
        }
        *endX = _polygonDisplayX(surface, last) + offsetX;
        *endY = _polygonDisplayY(surface, last) + offsetY;
        return;
    }

    SurfaceFrame frame = {.surface = surface,
                          .originX = 0.0,
                          .originY = _surfaceY,
                          .tangentX = 1.0,
                          .tangentY = 0.0,
                          .normalX = 0.0,
                          .normalY = 1.0,
                          .angleDegrees = 0.0};
    if (surface->type == SURFACE_TYPE_INCLINE && surface->hasAngle) {
        frame.angleDegrees = _angleToDegrees(surface->angle, surface->angleUnit);
        double radians = _degreesToRadians(frame.angleDegrees);
        frame.tangentX = cos(radians);
        frame.tangentY = sin(radians);
        frame.normalX = -sin(radians);
        frame.normalY = cos(radians);
    }

    double x0;
    double y0;
    double x1;
    double y1;
    if (!hasPreviousEndpoint) {
        double minimum;
        double maximum;
        _supportRange(&frame, layouts, count, &minimum, &maximum);
        x0 = frame.originX + minimum * frame.tangentX;
        y0 = frame.originY + minimum * frame.tangentY;
        x1 = frame.originX + maximum * frame.tangentX;
        y1 = frame.originY + maximum * frame.tangentY;
    } else {
        const double length = 6.0;
        x0 = previousEndX;
        y0 = previousEndY;
        x1 = x0 + length * frame.tangentX;
        y1 = y0 + length * frame.tangentY;
    }
    *endX = x1;
    *endY = y1;
    if (surface->friction != NULL) {
        char *staticCoefficient = _sourceTextToLatex(surface->friction->staticCoefficient.sourceText);
        char *kineticCoefficient = _sourceTextToLatex(surface->friction->kineticCoefficient.sourceText);
        _output("    \\draw[thick] (%f, %f) -- "
                "node[pos=0.68,sloped,below=1pt,inner sep=1pt] {\\scriptsize $F_s = %s,\\quad F_k = %s$} (%f, "
                "%f);\n",
                x0, y0, staticCoefficient, kineticCoefficient, x1, y1);
        free(staticCoefficient);
        free(kineticCoefficient);
    } else {
        _output("    \\draw[thick] (%f, %f) -- (%f, %f);\n", x0, y0, x1, y1);
    }

    if (surface->type == SURFACE_TYPE_INCLINE && surface->hasAngle) {
        char *angleLatex = _sourceTextToLatex(surface->angle.sourceText);
        char *unit = _angleUnitToString(surface->angleUnit);
        double signedAngle = _normalizeAngle(frame.angleDegrees);
        if (signedAngle > 180.0) { signedAngle -= 360.0; }
        double radius = 0.7;
        double labelAngle = signedAngle / 2.0;
        double labelRadians = _degreesToRadians(labelAngle);
        double labelRadius = radius + 0.22;
        _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", x0, y0, x0 + radius, y0);
        _output("    \\draw[thin] (%f, %f) arc[start angle=0,end angle=%f,radius=%f];\n", x0 + radius, y0, signedAngle,
                radius);
        _output("    \\node[anchor=%s] at (%f, %f) {\\scriptsize $%s%s$};\n", _labelAnchorForAngle(labelAngle),
                x0 + labelRadius * cos(labelRadians), y0 + labelRadius * sin(labelRadians), angleLatex, unit);
        free(angleLatex);
        free(unit);
    }
}

static void _generateSurfaces(System *system, BodyLayout *layouts, int count) {
    bool hasPreviousEndpoint = false;
    double previousEndX = 0.0;
    double previousEndY = 0.0;
    for (AstList *surface = system->surfaces; surface != NULL; surface = surface->next) {
        _generateSurface((Surface *) surface->value, layouts, count, hasPreviousEndpoint, previousEndX, previousEndY,
                         &previousEndX, &previousEndY);
        hasPreviousEndpoint = true;
    }
}

/* Bodies */

static void _generateBodies(System *system, BodyLayout *layouts, int count, const SurfaceFrame *frame) {
    (void) system;
    for (int i = 0; i < count; i++) { _generateBodyShape(layouts[i].body, &layouts[i]); }
    for (int i = 0; i < count; i++) { _generateBodyAnnotations(layouts[i].body, &layouts[i], layouts, count, frame); }
}

/* Distances */

static void _generateDistances(System *system, BodyLayout *layouts, int count) {
    int lane = 0;
    for (AstList *distanceNode = system->distances; distanceNode != NULL; distanceNode = distanceNode->next, lane++) {
        Distance *distance = (Distance *) distanceNode->value;
        int fromIndex = _findBodyLayout(layouts, count, distance->fromBodyName);
        int toIndex = _findBodyLayout(layouts, count, distance->toBodyName);
        if (fromIndex < 0 || toIndex < 0) { continue; }

        BodyLayout *from = &layouts[fromIndex];
        BodyLayout *to = &layouts[toIndex];
        double deltaX = to->x - from->x;
        double deltaY = to->y - from->y;
        double length = sqrt(deltaX * deltaX + deltaY * deltaY);
        if (length < 1e-9) { continue; }
        double perpendicularX = -deltaY / length;
        double perpendicularY = deltaX / length;
        if (perpendicularY > 0.0 || (fabs(perpendicularY) < 1e-9 && perpendicularX > 0.0)) {
            perpendicularX = -perpendicularX;
            perpendicularY = -perpendicularY;
        }
        double offset = 1.25 + lane * 0.55;
        double x1 = from->x + offset * perpendicularX;
        double y1 = from->y + offset * perpendicularY;
        double x2 = to->x + offset * perpendicularX;
        double y2 = to->y + offset * perpendicularY;
        double directionX = deltaX / length;
        double directionY = deltaY / length;
        _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", from->x, from->y, x1, y1);
        _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", to->x, to->y, x2, y2);
        _generateRightAngleMarker(x1, y1, -perpendicularX, -perpendicularY, directionX, directionY);
        _generateRightAngleMarker(x2, y2, -perpendicularX, -perpendicularY, -directionX, -directionY);

        if (distance->type == DISTANCE_TYPE_POLAR) {
            double angleDegrees = _angleToDegrees(distance->polar.angle, distance->polar.angleUnit);
            _generateForceAngle(x1, y1, 0.0, angleDegrees, distance->polar.angle, distance->polar.angleUnit);
        }

        char *label = NULL;
        if (distance->type == DISTANCE_TYPE_POLAR) {
            char *magnitude = _sourceTextToLatex(distance->polar.magnitude.sourceText);
            char *unit = _distanceUnitToString(distance->polar.magnitudeUnit);
            label = malloc(strlen(magnitude) + strlen(unit) + 3);
            sprintf(label, "$%s%s$", magnitude, unit);
            free(magnitude);
            free(unit);
        } else {
            char *x = _sourceTextToLatex(distance->cartesian.x.sourceText);
            char *y = _sourceTextToLatex(distance->cartesian.y.sourceText);
            char *xUnit = _distanceUnitToString(distance->cartesian.xUnit);
            char *yUnit = _distanceUnitToString(distance->cartesian.yUnit);
            size_t labelSize = strlen(x) + strlen(y) + strlen(xUnit) + strlen(yUnit) + 24;
            label = malloc(labelSize);
            snprintf(label, labelSize, "$x = %s%s$, $y = %s%s$", x, xUnit, y, yUnit);
            free(x);
            free(y);
            free(xUnit);
            free(yUnit);
        }
        _output("    \\draw[<->,thick] (%f, %f) -- node[midway,sloped,above=1pt,inner sep=1pt] {%s} (%f, %f);\n", x1,
                y1, label, x2, y2);
        free(label);
    }
}

/* Reference frame */

static void _generateReferenceFrame(ReferenceFrame *frame, BodyLayout *layouts, int count,
                                    const SurfaceFrame *surfaceFrame) {
    if (frame == NULL) { return; }
    int index = _findBodyLayout(layouts, count, frame->bodyName);
    if (index < 0) { return; }

    BodyLayout *body = &layouts[index];
    double xAxisAngle = frame->type == REFERENCE_FRAME_ALIGNED_WITH_SURFACE ? surfaceFrame->angleDegrees : 0.0;
    double xRadians = _degreesToRadians(xAxisAngle);
    double xDirectionX = cos(xRadians);
    double xDirectionY = sin(xRadians);
    double yDirectionX = -xDirectionY;
    double yDirectionY = xDirectionX;
    double originDirectionX = xDirectionX + yDirectionX;
    double originDirectionY = xDirectionY + yDirectionY;
    originDirectionX *= 0.7071067811865475;
    originDirectionY *= 0.7071067811865475;
    double originAngle = xAxisAngle + 45.0;
    double originDistance = _bodyRayExtent(body, originAngle) + 0.5;
    double originX = body->x + originDistance * originDirectionX;
    double originY = body->y + originDistance * originDirectionY;
    double axisLength = 1.25;
    double xEndX = originX + axisLength * xDirectionX;
    double xEndY = originY + axisLength * xDirectionY;
    double yEndX = originX + axisLength * yDirectionX;
    double yEndY = originY + axisLength * yDirectionY;

    _output("    \\draw[->,thick,blue] (%f, %f) -- (%f, %f);\n", originX, originY, xEndX, xEndY);
    _output("    \\node[blue,anchor=%s] at (%f, %f) {$x$};\n", _labelAnchorForAngle(xAxisAngle),
            xEndX + 0.12 * xDirectionX, xEndY + 0.12 * xDirectionY);
    _output("    \\draw[->,thick,blue] (%f, %f) -- (%f, %f);\n", originX, originY, yEndX, yEndY);
    _output("    \\node[blue,anchor=%s] at (%f, %f) {$y$};\n", _labelAnchorForAngle(xAxisAngle + 90.0),
            yEndX + 0.12 * yDirectionX, yEndY + 0.12 * yDirectionY);
}

/* System and program */

static void _generateSystem(System *system) {
    _output("  \\begin{tikzpicture}[scale=0.8,>=stealth]\n");

    int count = _countBodies(system);
    SurfaceFrame frame = _surfaceFrame(system);
    BodyLayout *layouts = _computeBodyLayouts(system, count, &frame);
    _applyPolarDistances(system, layouts, count);
    _expandForceCorridors(system, layouts, count, &frame);
    _assignMassLabelAngles(layouts, count, &frame);
    DiagramBounds bounds = _diagramBounds(system, layouts, count);

    _generateSurfaces(system, layouts, count);
    _generateBodies(system, layouts, count, &frame);
    _generateDistances(system, layouts, count);
    _generateReferenceFrame(system->referenceFrame, layouts, count, &frame);

    char *name = _escapeLatex(system->name);
    _output("    \\node[anchor=south] (system-title) at (%f, %f) {\\textbf{System: %s}};\n",
            (bounds.minX + bounds.maxX) / 2.0, bounds.maxY + 0.8, name);
    free(name);
    if (system->hasGravity) {
        char *gravity = _sourceTextToLatex(system->gravity.sourceText);
        _output("    \\draw[->,thin] ($(system-title.east)+(0.35,0.28)$) -- "
                "($(system-title.east)+(0.35,-0.28)$);\n");
        _output("    \\node[anchor=west] at ($(system-title.east)+(0.50,0)$) {\\scriptsize $g = %s$};\n", gravity);
        free(gravity);
    }
    _output("  \\end{tikzpicture}\n");
    free(layouts);
}

static void _generateProgram(Program *program) {
    if (program == NULL) { return; }
    for (AstList *system = program->systems; system != NULL; system = system->next) {
        _generateSystem((System *) system->value);
    }
}

/* PUBLIC FUNCTIONS */

void executeDiagramGenerator(Program *program) {
    logDebugging(_logger, "Generating TikZ diagram...");
    _generatePrologue();
    _generateProgram(program);
    _generateEpilogue();
    logDebugging(_logger, "Diagram generation is done.");
}

static void _generatePrologue(void) {
    _output("\\documentclass[border=5pt]{standalone}\n"
            "\\usepackage[utf8]{inputenc}\n"
            "\\usepackage[T1]{fontenc}\n"
            "\\usepackage{amsmath}\n"
            "\\usepackage{tikz}\n"
            "\\usetikzlibrary{arrows,calc}\n\n"
            "\\begin{document}\n");
}

static void _generateEpilogue(void) {
    _output("\\end{document}\n");
}
