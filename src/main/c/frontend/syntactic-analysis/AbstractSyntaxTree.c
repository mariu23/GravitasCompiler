#include "AbstractSyntaxTree.h"

#include <stdio.h>
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger *_logger = NULL;

/** Shutdown module's internal state. */
void _shutdownAbstractSyntaxTreeModule() {
    if (_logger != NULL) {
        logDebugging(_logger, "Destroying module: AbstractSyntaxTree...");
        destroyLogger(_logger);
        _logger = NULL;
    }
}

ModuleDestructor initializeAbstractSyntaxTreeModule() {
    _logger = createLogger("AbstractSyntaxTree");
    return _shutdownAbstractSyntaxTreeModule;
}

/* PRIVATE FUNCTIONS */

static void _destroyBodyValue(void *value);
static void _destroyDistanceValue(void *value);
static void _destroyForceValue(void *value);
static void _destroyImplicitForceValue(void *value);
static void _destroyPointValue(void *value);
static void _destroySystemValue(void *value);

static void _logAstDestructor(const char *functionName) {
    logDebugging(_logger, "Executing destructor: %s", functionName);
}

static void _destroyBodyValue(void *value) {
    destroyBody((Body *) value);
}

static void _destroyDistanceValue(void *value) {
    destroyDistance((Distance *) value);
}

static void _destroyForceValue(void *value) {
    destroyForce((Force *) value);
}

static void _destroyImplicitForceValue(void *value) {
    destroyImplicitForce((ImplicitForce *) value);
}

static void _destroyPointValue(void *value) {
    destroyPoint((Point *) value);
}

static void _destroySystemValue(void *value) {
    destroySystem((System *) value);
}

/* PUBLIC FUNCTIONS */

Value createValue(const double numericValue, const char *sourceText) {
    Value value;
    value.numericValue = numericValue;
    if (sourceText != NULL) {
        value.sourceText = malloc((strlen(sourceText) + 1) * sizeof(char));
        if (value.sourceText != NULL) { strcpy(value.sourceText, sourceText); }
    } else {
        value.sourceText = NULL;
    }
    return value;
}

void destroyValue(Value value) {
    if (value.sourceText != NULL) { free(value.sourceText); }
}

static char *_combineSourceTexts(const char *left, const char *op, const char *right) {
    const size_t leftLen = strlen(left);
    const size_t opLen = strlen(op);
    const size_t rightLen = strlen(right);
    char *result = malloc((leftLen + opLen + rightLen + 1) * sizeof(char));
    if (result != NULL) {
        strcpy(result, left);
        strcpy(result + leftLen, op);
        strcpy(result + leftLen + opLen, right);
    }
    return result;
}

Value negateValue(const Value v) {
    char *combined = _combineSourceTexts("-", "", v.sourceText);
    Value result = createValue(-v.numericValue, combined);
    free(combined);
    free(v.sourceText);
    return result;
}

Value multiplyValues(const Value a, const Value b) {
    char *combined = _combineSourceTexts(a.sourceText, "*", b.sourceText);
    Value result = createValue(a.numericValue * b.numericValue, combined);
    free(combined);
    free(a.sourceText);
    free(b.sourceText);
    return result;
}

Value divideValues(const Value a, const Value b) {
    char *combined = _combineSourceTexts(a.sourceText, "/", b.sourceText);
    Value result = createValue(a.numericValue / b.numericValue, combined);
    free(combined);
    free(a.sourceText);
    free(b.sourceText);
    return result;
}

Value valueFromParentheses(const Value inner) {
    char *combined = _combineSourceTexts("(", "", inner.sourceText);
    const size_t len = strlen(combined);
    char *wrapped = malloc((len + 2) * sizeof(char));
    if (wrapped != NULL) {
        strcpy(wrapped, combined);
        wrapped[len] = ')';
        wrapped[len + 1] = '\0';
    } else {
        wrapped = combined;
    }
    Value result = createValue(inner.numericValue, wrapped);
    free(wrapped);
    free(inner.sourceText);
    return result;
}

AstList *createAstList(void *value) {
    AstList *list = calloc(1, sizeof(AstList));
    if (list != NULL) { list->value = value; }
    return list;
}

AstList *appendAstList(AstList *list, void *value) {
    AstList *node = createAstList(value);
    if (list == NULL) { return node; }
    AstList *current = list;
    while (current->next != NULL) { current = current->next; }
    current->next = node;
    return list;
}

void destroyAstList(AstList *list, AstNodeDestructor destroyValue) {
    _logAstDestructor(__FUNCTION__);
    while (list != NULL) {
        AstList *next = list->next;
        if (destroyValue != NULL) { destroyValue(list->value); }
        free(list);
        list = next;
    }
}

void destroyBody(Body *body) {
    _logAstDestructor(__FUNCTION__);
    if (body != NULL) {
        free(body->name);
        body->name = NULL;
        free(body->parentBodyName);
        body->parentBodyName = NULL;
        destroyMass(body->mass);
        body->mass = NULL;
        destroyAstList(body->forces, _destroyForceValue);
        body->forces = NULL;
        destroyImplicitForceList(body->implicitForces);
        body->implicitForces = NULL;
        destroyFriction(body->friction);
        body->friction = NULL;
        free(body);
    }
}

void destroyDirection(Direction *direction) {
    _logAstDestructor(__FUNCTION__);
    if (direction != NULL) {
        destroyValue(direction->angle);
        free(direction);
    }
}

void destroyDistance(Distance *distance) {
    _logAstDestructor(__FUNCTION__);
    if (distance != NULL) {
        free(distance->fromBodyName);
        distance->fromBodyName = NULL;
        free(distance->toBodyName);
        distance->toBodyName = NULL;
        if (distance->type == DISTANCE_TYPE_POLAR) {
            destroyValue(distance->polar.magnitude);
            destroyValue(distance->polar.angle);
        } else {
            destroyValue(distance->cartesian.x);
            destroyValue(distance->cartesian.y);
        }
        free(distance);
    }
}

void destroyForce(Force *force) {
    _logAstDestructor(__FUNCTION__);
    if (force != NULL) {
        free(force->name);
        force->name = NULL;
        destroyValue(force->magnitude);
        destroyDirection(force->direction);
        force->direction = NULL;
        free(force);
    }
}

void destroyFriction(Friction *friction) {
    _logAstDestructor(__FUNCTION__);
    if (friction != NULL) {
        destroyValue(friction->staticCoefficient);
        destroyValue(friction->kineticCoefficient);
        free(friction);
    }
}

void destroyImplicitForce(ImplicitForce *implicitForce) {
    _logAstDestructor(__FUNCTION__);
    if (implicitForce != NULL) {
        free(implicitForce->name);
        implicitForce->name = NULL;
        free(implicitForce);
    }
}

void destroyImplicitForceList(ImplicitForceList *implicitForceList) {
    _logAstDestructor(__FUNCTION__);
    if (implicitForceList != NULL) {
        destroyAstList(implicitForceList->forces, _destroyImplicitForceValue);
        implicitForceList->forces = NULL;
        free(implicitForceList);
    }
}

void destroyMass(Mass *mass) {
    _logAstDestructor(__FUNCTION__);
    if (mass != NULL) {
        destroyValue(mass->value);
        free(mass);
    }
}

void destroyPoint(Point *point) {
    _logAstDestructor(__FUNCTION__);
    if (point != NULL) {
        destroyValue(point->x);
        destroyValue(point->y);
        free(point);
    }
}

void destroyProgram(Program *program) {
    _logAstDestructor(__FUNCTION__);
    if (program != NULL) {
        destroyAstList(program->systems, _destroySystemValue);
        program->systems = NULL;
        free(program);
    }
}

void destroyReferenceFrame(ReferenceFrame *referenceFrame) {
    _logAstDestructor(__FUNCTION__);
    if (referenceFrame != NULL) {
        free(referenceFrame->bodyName);
        referenceFrame->bodyName = NULL;
        free(referenceFrame);
    }
}

void destroySurface(Surface *surface) {
    _logAstDestructor(__FUNCTION__);
    if (surface != NULL) {
        destroyValue(surface->angle);
        destroyFriction(surface->friction);
        surface->friction = NULL;
        destroyAstList(surface->vertices, _destroyPointValue);
        surface->vertices = NULL;
        free(surface);
    }
}

void destroySystem(System *system) {
    _logAstDestructor(__FUNCTION__);
    if (system != NULL) {
        free(system->name);
        system->name = NULL;
        destroyValue(system->gravity);
        destroyUnits(system->units);
        system->units = NULL;
        destroyAstList(system->surfaces, (AstNodeDestructor) destroySurface);
        system->surfaces = NULL;
        destroyAstList(system->bodies, _destroyBodyValue);
        system->bodies = NULL;
        destroyReferenceFrame(system->referenceFrame);
        system->referenceFrame = NULL;
        destroyAstList(system->distances, _destroyDistanceValue);
        system->distances = NULL;
        free(system);
    }
}

void destroyUnits(Units *units) {
    _logAstDestructor(__FUNCTION__);
    if (units != NULL) { free(units); }
}
