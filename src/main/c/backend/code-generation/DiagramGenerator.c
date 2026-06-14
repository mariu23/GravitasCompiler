#include "DiagramGenerator.h"

#include <float.h>
#include <math.h>
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger *_logger = NULL;
static const double _pi = 3.14159265358979323846;
static const double _bodyGap = 1.4;
static const double _stackGap = 0.35;
static const double _surfaceClearance = 0.08;
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

/* PRIVATE TYPES */

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
static char *_escapeLatex(const char *text);
static char *_sourceTextToLatex(const char *sourceText);
static char *_angleUnitToString(AngleUnit unit);
static char *_distanceUnitToString(DistanceUnit unit);
static char *_forceUnitToString(ForceUnit unit);

static char *_bodySubscript(Body *body) {
    if (body == NULL || body->name == NULL || body->name[0] == '\0') { return NULL; }
    char initial[] = {body->name[0], '\0'};
    return _escapeLatex(initial);
}

/* Geometry and layout */

static double _degreesToRadians(double degrees) {
    return degrees * _pi / 180.0;
}

static double _angleToDegrees(Value angle, AngleUnit unit) {
    if (unit == ANGLE_UNIT_RADIAN) { return angle.numericValue * 180.0 / _pi; }
    return angle.numericValue;
}

static double _normalizeAngle(double degrees) {
    double normalized = degrees;
    while (normalized >= 360.0) { normalized -= 360.0; }
    while (normalized < 0.0) { normalized += 360.0; }
    return normalized;
}

static int _directionBucket(double degrees) {
    return ((int) ((_normalizeAngle(degrees) + 22.5) / 45.0)) % 8;
}

static const char *_labelAnchorForAngle(double degrees) {
    static const char *anchors[] = {"west", "south west", "south", "south east",
                                    "east", "north east", "north", "north west"};
    return anchors[_directionBucket(degrees)];
}

static const char *_labelAnchorForVector(double x, double y) {
    if (fabs(x) > 2.0 * fabs(y)) { return x >= 0.0 ? "west" : "east"; }
    if (fabs(y) > 2.0 * fabs(x)) { return y >= 0.0 ? "south" : "north"; }
    if (x >= 0.0 && y >= 0.0) { return "south west"; }
    if (x < 0.0 && y >= 0.0) { return "south east"; }
    if (x < 0.0 && y < 0.0) { return "north east"; }
    return "north west";
}

static int _countBodies(System *system) {
    int count = 0;
    for (AstList *item = system->bodies; item != NULL; item = item->next) { count++; }
    return count;
}

static int _findBodyLayout(BodyLayout *layouts, int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(layouts[i].body->name, name) == 0) { return i; }
    }
    return -1;
}

static void _bodyDimensions(Body *body, double *width, double *height) {
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

static double _bodyExtentAlong(const BodyLayout *layout, double directionX, double directionY) {
    if (layout->body->shape == BODY_SHAPE_SPHERE) { return layout->width / 2.0; }

    double rotation = _degreesToRadians(layout->rotation);
    double localX = directionX * cos(rotation) + directionY * sin(rotation);
    double localY = -directionX * sin(rotation) + directionY * cos(rotation);
    return fabs(localX) * layout->width / 2.0 + fabs(localY) * layout->height / 2.0;
}

static double _bodyRayExtent(const BodyLayout *layout, double angleDegrees) {
    if (layout->body->shape == BODY_SHAPE_SPHERE) { return layout->width / 2.0; }

    double localAngle = _degreesToRadians(angleDegrees - layout->rotation);
    double x = fabs(cos(localAngle));
    double y = fabs(sin(localAngle));
    double xExtent = x < 1e-9 ? DBL_MAX : layout->width / (2.0 * x);
    double yExtent = y < 1e-9 ? DBL_MAX : layout->height / (2.0 * y);
    return fmin(xExtent, yExtent);
}

static SurfaceFrame _surfaceFrame(System *system) {
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
    if (frame.surface->type == SURFACE_TYPE_INCLINE && frame.surface->hasAngle) {
        frame.angleDegrees = _angleToDegrees(frame.surface->angle, frame.surface->angleUnit);
        double radians = _degreesToRadians(frame.angleDegrees);
        frame.tangentX = cos(radians);
        frame.tangentY = sin(radians);
        frame.normalX = -sin(radians);
        frame.normalY = cos(radians);
    }
    return frame;
}

static double _polygonBoundaryY(Surface *surface, double x) {
    if (surface == NULL || surface->vertices == NULL) { return 0.0; }

    double highest = -DBL_MAX;
    AstList *firstNode = surface->vertices;
    AstList *node = firstNode;
    while (node != NULL) {
        AstList *nextNode = node->next != NULL ? node->next : firstNode;
        Point *a = (Point *) node->value;
        Point *b = (Point *) nextNode->value;
        double x1 = a->x.numericValue;
        double y1 = a->y.numericValue;
        double x2 = b->x.numericValue;
        double y2 = b->y.numericValue;

        if (fabs(x2 - x1) < 1e-9) {
            if (fabs(x - x1) < 1e-9) { highest = fmax(highest, fmax(y1, y2)); }
        } else if (x >= fmin(x1, x2) - 1e-9 && x <= fmax(x1, x2) + 1e-9) {
            double ratio = (x - x1) / (x2 - x1);
            highest = fmax(highest, y1 + ratio * (y2 - y1));
        }
        node = node->next;
    }

    if (highest != -DBL_MAX) { return highest; }
    for (AstList *vertex = surface->vertices; vertex != NULL; vertex = vertex->next) {
        Point *point = (Point *) vertex->value;
        highest = fmax(highest, point->y.numericValue);
    }
    return highest == -DBL_MAX ? 0.0 : highest;
}

static double _polygonCenterX(Surface *surface) {
    double minX = DBL_MAX;
    double maxX = -DBL_MAX;
    for (AstList *vertex = surface->vertices; vertex != NULL; vertex = vertex->next) {
        Point *point = (Point *) vertex->value;
        minX = fmin(minX, point->x.numericValue);
        maxX = fmax(maxX, point->x.numericValue);
    }
    return minX == DBL_MAX ? 0.0 : (minX + maxX) / 2.0;
}

static double _computeSubtreeSpan(BodyLayout *layouts, int count, int index) {
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

static void _positionChildren(BodyLayout *layouts, int count, int parentIndex, const SurfaceFrame *frame) {
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
        double normalOffset = parentExtent + childExtent + _stackGap;
        layouts[i].x = layouts[parentIndex].x + tangentOffset * frame->tangentX + normalOffset * frame->normalX;
        layouts[i].y = layouts[parentIndex].y + tangentOffset * frame->tangentY + normalOffset * frame->normalY;
        cursor += layouts[i].subtreeSpan + _bodyGap;
        _positionChildren(layouts, count, i, frame);
    }
}

static BodyLayout *_computeBodyLayouts(System *system, int count, SurfaceFrame *frame) {
    if (count == 0) { return NULL; }

    BodyLayout *layouts = calloc(count, sizeof(BodyLayout));
    int index = 0;
    for (AstList *item = system->bodies; item != NULL; item = item->next, index++) {
        layouts[index].body = (Body *) item->value;
        layouts[index].parentIndex = -1;
        _bodyDimensions(layouts[index].body, &layouts[index].width, &layouts[index].height);
        if (frame->surface != NULL && frame->surface->type == SURFACE_TYPE_INCLINE &&
            layouts[index].body->shape != BODY_SHAPE_SPHERE) {
            layouts[index].rotation = frame->angleDegrees;
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
    double polygonCenter =
        frame->surface != NULL && frame->surface->type == SURFACE_TYPE_POLYGON ? _polygonCenterX(frame->surface) : 0.0;
    for (int i = 0; i < count; i++) {
        if (layouts[i].parentIndex != -1) { continue; }
        double tangentPosition = cursor + layouts[i].subtreeSpan / 2.0;
        double supportX = frame->originX + tangentPosition * frame->tangentX;
        double supportY = frame->originY + tangentPosition * frame->tangentY;

        if (frame->surface == NULL) {
            layouts[i].x = tangentPosition;
            layouts[i].y = 0.0;
        } else if (frame->surface->type == SURFACE_TYPE_POLYGON) {
            supportX = polygonCenter + tangentPosition;
            supportY = _polygonBoundaryY(frame->surface, supportX);
            layouts[i].x = supportX;
            layouts[i].y = supportY + _bodyExtentAlong(&layouts[i], 0.0, 1.0) + _surfaceClearance;
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

static void _shiftBodySubtree(BodyLayout *layouts, int count, int index, double deltaX, double deltaY) {
    layouts[index].x += deltaX;
    layouts[index].y += deltaY;
    for (int i = 0; i < count; i++) {
        if (layouts[i].parentIndex == index) { _shiftBodySubtree(layouts, count, i, deltaX, deltaY); }
    }
}

static void _applyPolarDistances(System *system, BodyLayout *layouts, int count) {
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

static DiagramBounds _diagramBounds(System *system, BodyLayout *layouts, int count) {
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
            bounds.minX = fmin(bounds.minX, point->x.numericValue);
            bounds.maxX = fmax(bounds.maxX, point->x.numericValue);
            bounds.minY = fmin(bounds.minY, point->y.numericValue);
            bounds.maxY = fmax(bounds.maxY, point->y.numericValue);
        }
    }

    if (bounds.minX == DBL_MAX) { return (DiagramBounds){.minX = -3.0, .maxX = 3.0, .minY = -2.0, .maxY = 2.0}; }
    return bounds;
}

/* LaTeX helpers */

static char *_escapeLatex(const char *text) {
    if (text == NULL) { return strdup(""); }
    size_t capacity = strlen(text) * 16 + 1;
    char *escaped = calloc(capacity, 1);
    size_t position = 0;
    for (size_t i = 0; text[i] != '\0'; i++) {
        const char *replacement = NULL;
        char simple[3] = {'\\', text[i], '\0'};
        switch (text[i]) {
            case '#':
            case '$':
            case '%':
            case '&':
            case '_':
            case '{':
            case '}':
                replacement = simple;
                break;
            case '~':
                replacement = "\\textasciitilde{}";
                break;
            case '^':
                replacement = "\\textasciicircum{}";
                break;
            case '\\':
                replacement = "\\textbackslash{}";
                break;
            default:
                escaped[position++] = text[i];
                continue;
        }
        size_t length = strlen(replacement);
        memcpy(escaped + position, replacement, length);
        position += length;
    }
    return escaped;
}

static char *_sourceTextToLatex(const char *sourceText) {
    if (sourceText == NULL) { return strdup(""); }

    if (strcmp(sourceText, "PI") == 0) { return strdup("\\pi{}"); }
    if (strcmp(sourceText, "e") == 0 || strcmp(sourceText, "E") == 0) { return strdup("e"); }
    if (strcmp(sourceText, "PI/2") == 0) { return strdup("\\frac{\\pi}{2}"); }
    if (strcmp(sourceText, "-PI/2") == 0) { return strdup("-\\frac{\\pi}{2}"); }

    char *multipliedPi = strstr(sourceText, "*PI");
    if (multipliedPi != NULL && multipliedPi[3] == '\0') {
        size_t prefixLength = multipliedPi - sourceText;
        char *result = malloc(prefixLength + 8);
        snprintf(result, prefixLength + 8, "%.*s\\pi{}", (int) prefixLength, sourceText);
        return result;
    }

    if (strcmp(sourceText, "-PI") == 0) { return strdup("-\\pi{}"); }

    const char *exponentMarker = strchr(sourceText, 'e');
    if (exponentMarker == NULL) { exponentMarker = strchr(sourceText, 'E'); }
    if (exponentMarker != NULL && exponentMarker != sourceText) {
        size_t mantissaLength = exponentMarker - sourceText;
        char *mantissa = strndup(sourceText, mantissaLength);
        const char *exponent = exponentMarker + 1;
        if (*exponent == '+') { exponent++; }
        char *result = malloc(strlen(mantissa) + strlen(exponent) + 30);
        sprintf(result, "%s \\times 10^{%s}", mantissa, exponent);
        free(mantissa);
        return result;
    }

    return _escapeLatex(sourceText);
}

static char *_angleUnitToString(AngleUnit unit) {
    switch (unit) {
        case ANGLE_UNIT_DEGREE:
            return strdup("^\\circ");
        case ANGLE_UNIT_RADIAN:
            return strdup("\\,\\text{rad}");
        default:
            return strdup("");
    }
}

static char *_distanceUnitToString(DistanceUnit unit) {
    switch (unit) {
        case DISTANCE_UNIT_METER:
            return strdup("\\,\\text{m}");
        case DISTANCE_UNIT_CENTIMETER:
            return strdup("\\,\\text{cm}");
        case DISTANCE_UNIT_MILLIMETER:
            return strdup("\\,\\text{mm}");
        case DISTANCE_UNIT_KILOMETER:
            return strdup("\\,\\text{km}");
        default:
            return strdup("");
    }
}

static char *_forceUnitToString(ForceUnit unit) {
    switch (unit) {
        case FORCE_UNIT_NEWTON:
            return strdup("\\,\\text{N}");
        case FORCE_UNIT_KILONEWTON:
            return strdup("\\,\\text{kN}");
        default:
            return strdup("");
    }
}

static char *_massUnitToString(MassUnit unit) {
    switch (unit) {
        case MASS_UNIT_KG:
            return strdup("\\,\\text{kg}");
        case MASS_UNIT_GRAM:
            return strdup("\\,\\text{g}");
        case MASS_UNIT_MILLIGRAM:
            return strdup("\\,\\text{mg}");
        default:
            return strdup("");
    }
}

/* Surfaces */

static void _supportRange(const SurfaceFrame *frame, BodyLayout *layouts, int count, double *minimum, double *maximum) {
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

static void _generateSurface(Surface *surface, int index, BodyLayout *layouts, int count) {
    if (surface->type == SURFACE_TYPE_POLYGON) {
        if (surface->vertices == NULL) { return; }
        _output("    \\draw[thick] ");
        for (AstList *vertex = surface->vertices; vertex != NULL; vertex = vertex->next) {
            Point *point = (Point *) vertex->value;
            _output("(%f, %f)", point->x.numericValue, point->y.numericValue);
            if (vertex->next != NULL) { _output(" -- "); }
        }
        _output(" -- cycle;\n");
        return;
    }

    SurfaceFrame frame = {.surface = surface,
                          .originX = index == 0 ? 0.0 : index * 7.0,
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

    double minimum;
    double maximum;
    if (index == 0) {
        _supportRange(&frame, layouts, count, &minimum, &maximum);
    } else {
        minimum = -3.0;
        maximum = 3.0;
    }
    double x0 = frame.originX + minimum * frame.tangentX;
    double y0 = frame.originY + minimum * frame.tangentY;
    double x1 = frame.originX + maximum * frame.tangentX;
    double y1 = frame.originY + maximum * frame.tangentY;
    _output("    \\draw[thick] (%f, %f) -- (%f, %f);\n", x0, y0, x1, y1);

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

    if (surface->friction != NULL) {
        char *staticCoefficient = _sourceTextToLatex(surface->friction->staticCoefficient.sourceText);
        char *kineticCoefficient = _sourceTextToLatex(surface->friction->kineticCoefficient.sourceText);
        double midpoint = (minimum + maximum) / 2.0;
        double labelX = frame.originX + midpoint * frame.tangentX - 0.45 * frame.normalX;
        double labelY = frame.originY + midpoint * frame.tangentY - 0.45 * frame.normalY;
        _output("    \\node[anchor=north] at (%f, %f) {$F_s = %s,\\quad F_k = %s$};\n", labelX, labelY,
                staticCoefficient, kineticCoefficient);
        free(staticCoefficient);
        free(kineticCoefficient);
    }
}

static void _generateSurfaces(System *system, BodyLayout *layouts, int count) {
    int index = 0;
    for (AstList *surface = system->surfaces; surface != NULL; surface = surface->next, index++) {
        _generateSurface((Surface *) surface->value, index, layouts, count);
    }
}

/* Bodies and forces */

static double _resolveForceAngle(Force *force, const SurfaceFrame *frame) {
    if (force->direction->type == DIRECTION_TYPE_ABSOLUTE_ANGLE) {
        return _angleToDegrees(force->direction->angle, force->direction->angleUnit);
    }
    return frame->angleDegrees;
}

static double _implicitForceAngle(ImplicitForceType type, const SurfaceFrame *frame) {
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

static double _massLabelAngle(Body *body, const SurfaceFrame *frame, double preferredAngle) {
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

static void _assignMassLabelAngles(BodyLayout *layouts, int count, const SurfaceFrame *frame) {
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

static double _laneOffset(int lane) {
    if (lane == 0) { return 0.0; }
    double magnitude = ((lane + 1) / 2) * 0.32;
    return lane % 2 == 1 ? magnitude : -magnitude;
}

static bool _isAxisAligned(double angleDegrees) {
    double normalized = _normalizeAngle(angleDegrees);
    double remainder = normalized;
    while (remainder >= 90.0) { remainder -= 90.0; }
    return remainder < 1e-6 || 90.0 - remainder < 1e-6;
}

static void _generateForceAngle(double startX, double startY, double angleDegrees, Value angle, AngleUnit angleUnit) {
    if (_isAxisAligned(angleDegrees)) { return; }

    double signedAngle = _normalizeAngle(angleDegrees);
    if (signedAngle > 180.0) { signedAngle -= 360.0; }
    double radius = 0.55;
    double referenceX = startX + radius;
    double labelAngle = signedAngle / 2.0;
    double labelRadians = _degreesToRadians(labelAngle);
    double labelRadius = radius + 0.22;
    char *angleLatex = _sourceTextToLatex(angle.sourceText);
    char *unit = _angleUnitToString(angleUnit);

    _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", startX, startY, referenceX, startY);
    _output("    \\draw[thin] (%f, %f) arc[start angle=0,end angle=%f,radius=%f];\n", referenceX, startY, signedAngle,
            radius);
    _output("    \\node[anchor=%s] at (%f, %f) {\\scriptsize $%s%s$};\n", _labelAnchorForAngle(labelAngle),
            startX + labelRadius * cos(labelRadians), startY + labelRadius * sin(labelRadians), angleLatex, unit);
    free(angleLatex);
    free(unit);
}

static void _generateForceArrow(const BodyLayout *layout, double angleDegrees, const char *label, int *buckets,
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
    double endX = startX + _arrowLength * directionX;
    double endY = startY + _arrowLength * directionY;
    double labelX = endX + 0.18 * directionX + laneOffset * perpendicularX;
    double labelY = endY + 0.18 * directionY + laneOffset * perpendicularY;

    _output("    \\draw[->,thick%s] (%f, %f) -- (%f, %f);\n", style, startX, startY, endX, endY);
    _output("    \\node[anchor=%s] at (%f, %f) {%s};\n", _labelAnchorForAngle(angleDegrees), labelX, labelY, label);
}

static void _generateExplicitForces(Body *body, const BodyLayout *layout, const SurfaceFrame *frame, int *buckets) {
    for (AstList *forceNode = body->forces; forceNode != NULL; forceNode = forceNode->next) {
        Force *force = (Force *) forceNode->value;
        char *name = _escapeLatex(force->name);
        char *magnitude = _sourceTextToLatex(force->magnitude.sourceText);
        char *unit = _forceUnitToString(force->unit);
        size_t labelSize = strlen(name) + strlen(magnitude) + strlen(unit) + 16;
        char *label = malloc(labelSize);
        snprintf(label, labelSize, "$%s = %s%s$", name, magnitude, unit);
        double angleDegrees = _resolveForceAngle(force, frame);
        _generateForceArrow(layout, angleDegrees, label, buckets, "");
        if (force->direction->type == DIRECTION_TYPE_ABSOLUTE_ANGLE) {
            double startDistance = _bodyRayExtent(layout, angleDegrees) + 0.04;
            double radians = _degreesToRadians(angleDegrees);
            double startX = layout->x + startDistance * cos(radians);
            double startY = layout->y + startDistance * sin(radians);
            _generateForceAngle(startX, startY, angleDegrees, force->direction->angle, force->direction->angleUnit);
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

static void _generateImplicitForces(Body *body, const BodyLayout *layout, const SurfaceFrame *frame, int *buckets) {
    if (body->implicitForces == NULL) { return; }
    for (AstList *forceNode = body->implicitForces->forces; forceNode != NULL; forceNode = forceNode->next) {
        ImplicitForce *force = (ImplicitForce *) forceNode->value;
        double angle = _implicitForceAngle(force->type, frame);
        char *label = _implicitForceLabel(force, body);
        _generateForceArrow(layout, angle, label, buckets, "");
        free(label);
    }
}

static void _generateBody(Body *body, const BodyLayout *layout, const SurfaceFrame *frame) {
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
    _generateExplicitForces(body, layout, frame, buckets);
    _generateImplicitForces(body, layout, frame, buckets);
}

static void _generateBodies(System *system, BodyLayout *layouts, int count, const SurfaceFrame *frame) {
    (void) system;
    for (int i = 0; i < count; i++) { _generateBody(layouts[i].body, &layouts[i], frame); }
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
        _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", from->x, from->y, x1, y1);
        _output("    \\draw[thin,densely dashed] (%f, %f) -- (%f, %f);\n", to->x, to->y, x2, y2);

        if (distance->type == DISTANCE_TYPE_POLAR) {
            double angleDegrees = _angleToDegrees(distance->polar.angle, distance->polar.angleUnit);
            _generateForceAngle(x1, y1, angleDegrees, distance->polar.angle, distance->polar.angleUnit);
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
    _assignMassLabelAngles(layouts, count, &frame);
    DiagramBounds bounds = _diagramBounds(system, layouts, count);

    _generateSurfaces(system, layouts, count);
    _generateBodies(system, layouts, count, &frame);
    _generateDistances(system, layouts, count);
    _generateReferenceFrame(system->referenceFrame, layouts, count, &frame);

    char *name = _escapeLatex(system->name);
    _output("    \\node[anchor=south] at (%f, %f) {\\textbf{System: %s}};\n", (bounds.minX + bounds.maxX) / 2.0,
            bounds.maxY + 0.8, name);
    free(name);
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

static void _output(const char *const format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vfprintf(stdout, format, arguments);
    fflush(stdout);
    va_end(arguments);
}
