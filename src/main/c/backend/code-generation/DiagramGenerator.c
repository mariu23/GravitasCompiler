#include "DiagramGenerator.h"

#include <math.h>
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger *_logger = NULL;
static const double _bodySpacing = 4.0;
static const double _bodyStackOffset = 2.5;
static const double _surfaceY = -1.5;
static const double _arrowLength = 2.0;

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

typedef struct {
    char *name;
    double x;
    double y;
    int parentIndex;// -1 = no parent
} BodyPosition;

static void _generatePrologue(void);
static void _generateEpilogue(void);
static void _generateSystem(System *system);
static void _generateSurfaces(System *system);
static void _generateSurface(Surface *surface, int index);
static void _generateBodies(System *system, BodyPosition *positions);
static void _generateBody(Body *body, BodyPosition *pos, System *system, BodyPosition *allPositions, int allCount);
static void _generateExplicitForces(Body *body, BodyPosition *pos, System *system);
static void _generateExplicitForce(Force *force, BodyPosition *pos, System *system);
static void _generateImplicitForces(Body *body, BodyPosition *pos, System *system);
static void _generateDistances(System *system, BodyPosition *positions, int count);
static void _generateReferenceFrame(ReferenceFrame *frame, System *system, BodyPosition *positions, int count);
static void _output(const char *const format, ...);
static char *_sourceTextToLatex(const char *sourceText);
static char *_angleUnitToString(AngleUnit unit);
static char *_distanceUnitToString(DistanceUnit unit);
static char *_forceUnitToString(ForceUnit unit);

/* Layout */

static int _countBodies(System *system) {
    int count = 0;
    for (AstList *b = system->bodies; b != NULL; b = b->next) { count++; }
    return count;
}

static int _findBodyPosition(BodyPosition *positions, int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(positions[i].name, name) == 0) { return i; }
    }
    return -1;
}

static void _computePositions(System *system, BodyPosition *positions, int count) {
    int index = 0;
    for (AstList *b = system->bodies; b != NULL; b = b->next, index++) {
        Body *body = (Body *) b->value;
        positions[index].name = body->name;
        positions[index].parentIndex = -1;

        if (body->parentBodyName != NULL) {
            int parentIdx = _findBodyPosition(positions, index, body->parentBodyName);
            if (parentIdx >= 0) {
                positions[index].x = positions[parentIdx].x;
                positions[index].y = positions[parentIdx].y + _bodyStackOffset;
                positions[index].parentIndex = parentIdx;
                continue;
            }
        }
        // default position
        positions[index].x = 2.0 + index * _bodySpacing;
        positions[index].y = 0.0;
    }
}

/* latex helpers */

static char *_escapeLatex(const char *s) {
    if (s == NULL) { return strdup(""); }
    size_t len = strlen(s);
    // handle # $ % & ~ _ ^ \ { }
    size_t cap = len * 2 + 1;
    char *out = calloc(cap, 1);
    size_t j = 0;
    for (size_t i = 0; s[i] != '\0' && j < cap - 2; i++) {
        switch (s[i]) {
            case '#':
            case '$':
            case '%':
            case '&':
            case '_':
            case '{':
            case '}':
                out[j++] = '\\';
                out[j++] = s[i];
                break;
            case '~':
                out[j++] = '\\';
                out[j++] = 't';
                out[j++] = 'i';
                out[j++] = 'l';
                out[j++] = 'd';
                out[j++] = 'e';
                break;
            case '^':
                out[j++] = '\\';
                out[j++] = '^';
                break;
            case '\\':
                out[j++] = '\\';
                out[j++] = 't';
                out[j++] = 'e';
                out[j++] = 'x';
                out[j++] = 't';
                out[j++] = 'b';
                out[j++] = 'a';
                out[j++] = 'c';
                out[j++] = 'k';
                out[j++] = 's';
                out[j++] = 'l';
                out[j++] = 'a';
                out[j++] = 's';
                out[j++] = 'h';
                break;
            default:
                out[j++] = s[i];
                break;
        }
    }
    return out;
}

static char *_sourceTextToLatex(const char *sourceText) {
    if (sourceText == NULL) { return strdup(""); }

    if (strcmp(sourceText, "PI") == 0) { return strdup("\\pi{}"); }
    if (strcmp(sourceText, "e") == 0 || strcmp(sourceText, "E") == 0) { return strdup("e"); }

    if (strcmp(sourceText, "PI/2") == 0) { return strdup("\\frac{\\pi}{2}"); }
    if (strcmp(sourceText, "-PI/2") == 0) { return strdup("-\\frac{\\pi}{2}"); }

    char *mulPi = strstr(sourceText, "*PI");
    if (mulPi != NULL && mulPi[3] == '\0') {
        char prefix[32];
        size_t len = mulPi - sourceText;
        if (len < sizeof(prefix)) {
            strncpy(prefix, sourceText, len);
            prefix[len] = '\0';
            char *result = malloc(strlen(prefix) + 20);
            sprintf(result, "%s\\pi{}", prefix);
            return result;
        }
    }

    if (strcmp(sourceText, "-PI") == 0) { return strdup("-\\pi{}"); }

    const char *e = strchr(sourceText, 'e');
    if (e == NULL) { e = strchr(sourceText, 'E'); }
    if (e != NULL && e != sourceText) {
        size_t mantLen = e - sourceText;
        char *mant = strndup(sourceText, mantLen);
        const char *exp = e + 1;
        if (*exp == '+' || *exp == '-') { exp++; }
        char *result = malloc(strlen(mant) + strlen(exp) + 30);
        sprintf(result, "%s \\times 10^{%s}", mant, exp);
        free(mant);
        return result;
    }

    const char *star = strchr(sourceText, '*');
    if (star != NULL && (star[1] == 'P' || star[1] == 'e' || star[1] == 'E')) {}

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

static void _generateSurface(Surface *surface, int index) {
    switch (surface->type) {
        case SURFACE_TYPE_HORIZONTAL: {
            double x0 = index * 6.0 - 2.0;
            double x1 = x0 + 6.0;
            _output("    \\draw[thick] (%f, %f) -- (%f, %f);\n", x0, _surfaceY, x1, _surfaceY);
            break;
        }
        case SURFACE_TYPE_INCLINE: {
            double angle = surface->hasAngle ? surface->angle.numericValue : 0.0;
            double radians = angle * 3.1415926535 / 180.0;
            double len = 4.0;
            double x0 = index * 6.0 - 1.0;
            double y0 = _surfaceY;
            double x1 = x0 + len * cos(radians);
            double y1 = y0 + len * sin(radians);
            _output("    \\draw[thick] (%f, %f) -- (%f, %f);\n", x0, y0, x1, y1);
            if (surface->hasAngle) {
                char *angleLatex = _sourceTextToLatex(surface->angle.sourceText);
                char *unitStr = _angleUnitToString(surface->angleUnit);
                _output("    \\node at (%f, %f) {$%s%s$};\n", (x0 + x1) / 2, (y0 + y1) / 2 + 0.3, angleLatex, unitStr);
                free(angleLatex);
                free(unitStr);
            }
            break;
        }
        case SURFACE_TYPE_POLYGON: {
            if (surface->vertices == NULL) { break; }
            _output("    \\draw[thick] ");
            for (AstList *v = surface->vertices; v != NULL; v = v->next) {
                Point *pt = (Point *) v->value;
                _output("(%f, %f)", pt->x.numericValue, pt->y.numericValue);
                if (v->next != NULL) { _output(" -- "); }
            }
            _output(" -- cycle;\n");
            break;
        }
    }
}

static void _generateSurfaces(System *system) {
    if (system->surfaces == NULL) { return; }
    int index = 0;
    for (AstList *s = system->surfaces; s != NULL; s = s->next, index++) {
        Surface *surface = (Surface *) s->value;
        _generateSurface(surface, index);
    }
}

/* Forces */

static double _resolveForceAngle(Force *force, System *system) {
    if (force->direction->type == DIRECTION_TYPE_ABSOLUTE_ANGLE) { return force->direction->angle.numericValue; }
    if (system->surfaces != NULL) {
        Surface *first = (Surface *) system->surfaces->value;
        if (first->hasAngle) { return first->angle.numericValue; }
    }
    return 0.0;
}

static void _generateExplicitForce(Force *force, BodyPosition *pos, System *system) {
    double angleDeg = _resolveForceAngle(force, system);
    double radians = angleDeg * 3.1415926535 / 180.0;
    double dx = _arrowLength * cos(radians);
    double dy = _arrowLength * sin(radians);
    double startX = pos->x;
    double startY = pos->y;

    char *magnitudeLatex = _sourceTextToLatex(force->magnitude.sourceText);
    char *forceUnitStr = _forceUnitToString(force->unit);
    char *nameEscaped = _escapeLatex(force->name);

    _output("    \\draw[->,thick] (%f, %f) -- ++(%f, %f) node[above,midway] {$%s = %s%s$};\n", startX, startY, dx, dy,
            nameEscaped, magnitudeLatex, forceUnitStr);

    free(magnitudeLatex);
    free(forceUnitStr);
    free(nameEscaped);
}

static void _generateExplicitForces(Body *body, BodyPosition *pos, System *system) {
    if (body->forces == NULL) { return; }
    for (AstList *f = body->forces; f != NULL; f = f->next) {
        Force *force = (Force *) f->value;
        _generateExplicitForce(force, pos, system);
    }
}

static void _generateImplicitForces(Body *body, BodyPosition *pos, System *system) {
    if (body->implicitForces == NULL || body->implicitForces->forces == NULL) { return; }
    for (AstList *f = body->implicitForces->forces; f != NULL; f = f->next) {
        ImplicitForce *imp = (ImplicitForce *) f->value;
        char *label = NULL;


        switch (imp->type) {
            case IMPLICIT_FORCE_WEIGHT: {
                double downY = pos->y - _arrowLength;
                char *name = imp->name ? _escapeLatex(imp->name) : NULL;
                if (name) {
                    label = malloc(strlen(name) + 10);
                    sprintf(label, "$%s$", name);
                    free(name);
                } else {
                    label = strdup("$W$");
                }
                _output("    \\draw[->,thick] (%f, %f) -- (%f, %f) node[right,midway] {%s};\n", pos->x, pos->y, pos->x,
                        downY, label);
                break;
            }
            case IMPLICIT_FORCE_NORMAL: {
                double upY = pos->y + _arrowLength;
                char *name = imp->name ? _escapeLatex(imp->name) : NULL;
                if (name) {
                    label = malloc(strlen(name) + 10);
                    sprintf(label, "$%s$", name);
                    free(name);
                } else {
                    label = strdup("$N$");
                }
                _output("    \\draw[->,thick] (%f, %f) -- (%f, %f) node[right,midway] {%s};\n", pos->x, pos->y, pos->x,
                        upY, label);
                break;
            }
            case IMPLICIT_FORCE_FRICTION: {
                double rightX = pos->x + _arrowLength;
                char *name = imp->name ? _escapeLatex(imp->name) : NULL;
                if (name) {
                    label = malloc(strlen(name) + 10);
                    sprintf(label, "$%s$", name);
                    free(name);
                } else {
                    label = strdup("$F_f$");
                }
                _output("    \\draw[->,thick] (%f, %f) -- (%f, %f) node[above,midway] {%s};\n", pos->x, pos->y, rightX,
                        pos->y, label);
                break;
            }
        }

        if (label) { free(label); }
    }
}

/* Bodies */

static void _generateBody(Body *body, BodyPosition *pos, System *system, BodyPosition *allPositions, int allCount) {
    char *nameEscaped = _escapeLatex(body->name);
    double x = pos->x;
    double y = pos->y;
    double w = 1.0;
    double h = 0.8;

    switch (body->shape) {
        case BODY_SHAPE_SPHERE:
            _output("    \\draw[thick,fill=gray!20] (%f, %f) circle (0.6) node {$%s$};\n", x, y, nameEscaped);
            break;
        case BODY_SHAPE_BLOCK:
        case BODY_SHAPE_DEFAULT:
        default:
            _output("    \\draw[thick,fill=gray!20] (%f, %f) rectangle (%f, %f) node[midway] {$%s$};\n", x - w / 2,
                    y - h / 2, x + w / 2, y + h / 2, nameEscaped);
            break;
    }
    free(nameEscaped);

    if (body->mass != NULL) {
        char *massLatex = _sourceTextToLatex(body->mass->value.sourceText);
        char *massUnitStr = _massUnitToString(body->mass->unit);
        _output("    \\node at (%f, %f) {\\scriptsize $m = %s%s$};\n", x, y - h / 2 - 0.3, massLatex, massUnitStr);
        free(massLatex);
        free(massUnitStr);
    }

    _generateExplicitForces(body, pos, system);
    _generateImplicitForces(body, pos, system);
}

static void _generateBodies(System *system, BodyPosition *positions) {
    int count = _countBodies(system);
    int index = 0;
    for (AstList *b = system->bodies; b != NULL; b = b->next, index++) {
        Body *body = (Body *) b->value;
        _generateBody(body, &positions[index], system, positions, count);
    }
}

/* Distances */

static void _generateDistances(System *system, BodyPosition *positions, int count) {
    if (system->distances == NULL) { return; }
    for (AstList *d = system->distances; d != NULL; d = d->next) {
        Distance *dist = (Distance *) d->value;
        int fromIdx = _findBodyPosition(positions, count, dist->fromBodyName);
        int toIdx = _findBodyPosition(positions, count, dist->toBodyName);
        if (fromIdx < 0 || toIdx < 0) { continue; }

        double x1 = positions[fromIdx].x, y1 = positions[fromIdx].y;
        double x2 = positions[toIdx].x, y2 = positions[toIdx].y;

        _output("    \\draw[<->,thick] (%f, %f) -- (%f, %f);\n", x1, y1, x2, y2);

        if (dist->type == DISTANCE_TYPE_POLAR) {
            char *magLatex = _sourceTextToLatex(dist->polar.magnitude.sourceText);
            char *unitStr = _distanceUnitToString(dist->polar.magnitudeUnit);
            _output("    \\node at (%f, %f) {$%s%s$};\n", (x1 + x2) / 2, (y1 + y2) / 2 + 0.3, magLatex, unitStr);
            free(magLatex);
            free(unitStr);
        } else {
            char *xLatex = _sourceTextToLatex(dist->cartesian.x.sourceText);
            char *yLatex = _sourceTextToLatex(dist->cartesian.y.sourceText);
            char *xUnitStr = _distanceUnitToString(dist->cartesian.xUnit);
            char *yUnitStr = _distanceUnitToString(dist->cartesian.yUnit);
            _output("    \\node at (%f, %f) {$x = %s%s$, $y = %s%s$};\n", (x1 + x2) / 2, (y1 + y2) / 2 + 0.3, xLatex,
                    xUnitStr, yLatex, yUnitStr);
            free(xLatex);
            free(yLatex);
            free(xUnitStr);
            free(yUnitStr);
        }
    }
}

/* Reference Frame */

static void _generateReferenceFrame(ReferenceFrame *frame, System *system, BodyPosition *positions, int count) {
    if (frame == NULL) { return; }
    int idx = _findBodyPosition(positions, count, frame->bodyName);
    if (idx < 0) { return; }
    double x = positions[idx].x;
    double y = positions[idx].y;
    double axisLen = 1.0;

    if (frame->type == REFERENCE_FRAME_ALIGNED_WITH_SURFACE) {
        double angle = 0.0;
        if (system->surfaces != NULL) {
            Surface *first = (Surface *) system->surfaces->value;
            if (first->hasAngle) { angle = first->angle.numericValue; }
        }
        double rad = angle * 3.1415926535 / 180.0;
        double dx = axisLen * cos(rad);
        double dy = axisLen * sin(rad);
        _output("    \\draw[->,thick,blue] (%f, %f) -- ++(%f, %f) node[above] {$x$};\n", x, y, dx, dy);
        _output("    \\draw[->,thick,blue] (%f, %f) -- ++(%f, %f) node[right] {$y$};\n", x, y, -dy, dx);
    } else {
        _output("    \\draw[->,thick,blue] (%f, %f) -- ++(%f, %f) node[above] {$x$};\n", x, y, axisLen, 0.0);
        _output("    \\draw[->,thick,blue] (%f, %f) -- ++(%f, %f) node[right] {$y$};\n", x, y, 0.0, axisLen);
    }
}

/* System */

static void _generateSystem(System *system) {
    _output("  \\begin{tikzpicture}[scale=0.8]\n");
    _output("    \\coordinate (origin) at (0,0);\n");

    int count = _countBodies(system);
    BodyPosition *positions = NULL;
    if (count > 0) {
        positions = calloc(count, sizeof(BodyPosition));
        _computePositions(system, positions, count);
    }

    _generateSurfaces(system);
    _generateBodies(system, positions);
    _generateDistances(system, positions, count);
    if (system->referenceFrame != NULL) { _generateReferenceFrame(system->referenceFrame, system, positions, count); }

    char *nameEscaped = _escapeLatex(system->name);
    _output("    \\node[above] at (0, %f) {\\textbf{System: %s}};\n", 8.0, nameEscaped);
    free(nameEscaped);

    _output("  \\end{tikzpicture}\n");

    free(positions);
}

/* Program */

static void _generateProgram(Program *program) {
    if (program == NULL) { return; }
    for (AstList *s = program->systems; s != NULL; s = s->next) {
        System *system = (System *) s->value;
        _generateSystem(system);
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
