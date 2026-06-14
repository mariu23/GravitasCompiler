#include "SemanticAnalyzer.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* MODULE INTERNAL STATE */

static Logger *_logger = NULL;

/** Shutdown module's internal state. */
void _shutdownSemanticAnalyzerModule() {
    if (_logger != NULL) {
        logDebugging(_logger, "Destroying module: SemanticAnalyzer...");
        destroyLogger(_logger);
        _logger = NULL;
    }
}

ModuleDestructor initializeSemanticAnalyzerModule() {
    _logger = createLogger("SemanticAnalyzer");
    return _shutdownSemanticAnalyzerModule;
}

/* PRIVATE FUNCTIONS */

/* Symbol table functions */

typedef struct {
    char **names;
    int count;
} BodyTable;

static void initBodyTable(BodyTable *table, int capacity) {
    table->names = calloc(capacity, sizeof(char *));
    table->count = 0;
}

static void destroyBodyTable(BodyTable *table) {
    if (table == NULL) { return; }
    if (table->names != NULL) {
        for (int i = 0; i < table->count; ++i) { free(table->names[i]); }
        free(table->names);
        table->names = NULL;
    }
    table->count = 0;
    free(table);
}

static BodyTable *collectBodyNames(AstList *bodies) {
    int count = 0;
    for (AstList *b = bodies; b != NULL; b = b->next) { count++; }
    BodyTable *table = calloc(1, sizeof(BodyTable));
    if (table == NULL) { return NULL; }
    initBodyTable(table, count);
    int i = 0;
    for (AstList *b = bodies; b != NULL; b = b->next) {
        Body *body = (Body *) b->value;
        table->names[i] = strdup(body->name);
        i++;
    }
    table->count = i;
    return table;
}

static bool findBodyByName(BodyTable *table, const char *name) {
    for (int i = 0; i < table->count; ++i) {
        if (strcmp(table->names[i], name) == 0) { return true; }
    }
    return false;
}

static bool isBodyNameDuplicate(BodyTable *table, const char *name) {
    int count = 0;
    for (int i = 0; i < table->count && count < 2; ++i) {
        if (strcmp(table->names[i], name) == 0) { count++; }
    }
    return count > 1;
}

/* Validation helpers */

static bool hasSurfaceWithFriction(AstList *surfaces) {
    for (AstList *s = surfaces; s != NULL; s = s->next) {
        Surface *surface = (Surface *) s->value;
        if (surface->friction != NULL) { return true; }
    }
    return false;
}

static bool hasAnySurface(AstList *surfaces) {
    return surfaces != NULL;
}

static bool hasBodyWithFriction(Body *body) {
    return body->friction != NULL;
}

static bool isCyclic(Body *body, BodyTable *table, AstList *bodies) {
    if (body->parentBodyName == NULL) { return false; }

    const char *current = body->parentBodyName;
    int visited = 0;
    while (current != NULL && visited < table->count) {
        if (strcmp(current, body->name) == 0) {
            return true;// looped?
        }

        Body *parent = NULL;
        for (AstList *b = bodies; b != NULL && parent == NULL; b = b->next) {
            Body *candidate = (Body *) b->value;
            if (strcmp(candidate->name, current) == 0) { parent = candidate; }
        }
        if (parent == NULL) { break; }
        current = parent->parentBodyName;
        visited++;
    }
    return false;
}

static CompilationStatus validateMass(Mass *mass) {
    if (mass == NULL) { return SUCCEEDED; }
    if (mass->value.numericValue <= 0.0) {
        logError(_logger, "Mass must be positive (got %s).", mass->value.sourceText);
        return FAILED;
    }
    return SUCCEEDED;
}

static CompilationStatus validateGravity(Value gravity, bool hasGravity) {
    if (!hasGravity) { return SUCCEEDED; }
    if (gravity.numericValue < 0.0) {
        logError(_logger, "Gravity must be non-negative (got %s).", gravity.sourceText);
        return FAILED;
    }
    return SUCCEEDED;
}

static CompilationStatus validateForceMagnitude(Force *force) {
    if (force->magnitude.numericValue <= 0.0) {
        logError(_logger, "Force \"%s\" magnitude must be positive (got %s).", force->name,
                 force->magnitude.sourceText);
        return FAILED;
    }
    return SUCCEEDED;
}

static CompilationStatus validateDistanceMagnitudes(AstList *distances) {
    for (AstList *d = distances; d != NULL; d = d->next) {
        Distance *distance = (Distance *) d->value;
        if (distance->type == DISTANCE_TYPE_POLAR && distance->polar.magnitude.numericValue < 0.0) {
            logError(_logger, "Distance magnitude must be non-negative (got %s).",
                     distance->polar.magnitude.sourceText);
            return FAILED;
        }
    }
    return SUCCEEDED;
}

static CompilationStatus validateFrictionCoefficients(Friction *friction) {
    if (friction == NULL) { return SUCCEEDED; }
    if (friction->staticCoefficient.numericValue < 0) {
        logError(_logger, "Friction static coefficient must be non-negative.");
        return FAILED;
    }
    if (friction->kineticCoefficient.numericValue < 0) {
        logError(_logger, "Friction kinetic coefficient must be non-negative.");
        return FAILED;
    }
    return SUCCEEDED;
}

static CompilationStatus validateBodyNames(BodyTable *table, AstList *bodies) {
    for (int i = 0; i < table->count; ++i) {
        if (isBodyNameDuplicate(table, table->names[i])) {
            logError(_logger, "Duplicate body name: \"%s\".", table->names[i]);
            return FAILED;
        }
    }
    return SUCCEEDED;
}

static CompilationStatus validateParentBodies(BodyTable *table, AstList *bodies) {
    for (AstList *b = bodies; b != NULL; b = b->next) {
        Body *body = (Body *) b->value;
        if (body->parentBodyName != NULL && !findBodyByName(table, body->parentBodyName)) {
            logError(_logger, "Parent body \"%s\" not found for body \"%s\".", body->parentBodyName, body->name);
            return FAILED;
        }
    }
    return SUCCEEDED;
}

static CompilationStatus validateNoCyclicBodies(BodyTable *table, AstList *bodies) {
    for (AstList *b = bodies; b != NULL; b = b->next) {
        Body *body = (Body *) b->value;
        if (isCyclic(body, table, bodies)) {
            logError(_logger, "Cyclic body dependency detected for body \"%s\".", body->name);
            return FAILED;
        }
    }
    return SUCCEEDED;
}

static CompilationStatus validatePolygonVertices(AstList *surfaces) {
    for (AstList *s = surfaces; s != NULL; s = s->next) {
        Surface *surface = (Surface *) s->value;
        if (surface->type != SURFACE_TYPE_POLYGON) { continue; }

        int count = 0;
        for (AstList *v = surface->vertices; v != NULL && count < 3; v = v->next) { count++; }
        if (count < 3) {
            logError(_logger, "Polygon surface must have at least 3 vertices, got %d.", count);
            return FAILED;
        }
    }
    return SUCCEEDED;
}

static CompilationStatus validateDistanceBodies(AstList *distances, BodyTable *table) {
    for (AstList *d = distances; d != NULL; d = d->next) {
        Distance *distance = (Distance *) d->value;
        if (!findBodyByName(table, distance->fromBodyName)) {
            logError(_logger, "Distance references unknown body \"%s\".", distance->fromBodyName);
            return FAILED;
        }
        if (!findBodyByName(table, distance->toBodyName)) {
            logError(_logger, "Distance references unknown body \"%s\".", distance->toBodyName);
            return FAILED;
        }
    }
    return SUCCEEDED;
}

static CompilationStatus validateReferenceFrame(ReferenceFrame *frame, BodyTable *table) {
    if (frame == NULL) { return SUCCEEDED; }
    if (!findBodyByName(table, frame->bodyName)) {
        logError(_logger, "Reference frame references unknown body \"%s\".", frame->bodyName);
        return FAILED;
    }
    return SUCCEEDED;
}

static CompilationStatus validateForceNames(AstList *forces) {
    for (AstList *outer = forces; outer != NULL; outer = outer->next) {
        Force *a = (Force *) outer->value;
        for (AstList *inner = outer->next; inner != NULL; inner = inner->next) {
            Force *b = (Force *) inner->value;
            if (strcmp(a->name, b->name) == 0) {
                logError(_logger, "Duplicate force name \"%s\" within body.", a->name);
                return FAILED;
            }
        }
    }
    return SUCCEEDED;
}

static CompilationStatus validateSurfaceDependentDirections(AstList *forces, AstList *surfaces) {
    bool hasSurface = hasAnySurface(surfaces);
    for (AstList *f = forces; f != NULL; f = f->next) {
        Force *force = (Force *) f->value;
        if (force->direction == NULL) { continue; }
        if (force->direction->type == DIRECTION_TYPE_PARALLEL_TO_SURFACE && !hasSurface) {
            logError(_logger, "Direction parallel to surface requires at least one surface in the system.");
            return FAILED;
        }
    }
    return SUCCEEDED;
}

static CompilationStatus validateImplicitForces(ImplicitForceList *implicitForces, AstList *surfaces, Body *body) {
    if (implicitForces == NULL || implicitForces->forces == NULL) { return SUCCEEDED; }

    bool hasSurface = hasAnySurface(surfaces);
    bool hasSurfaceFric = hasSurfaceWithFriction(surfaces);

    for (AstList *f = implicitForces->forces; f != NULL; f = f->next) {
        ImplicitForce *imp = (ImplicitForce *) f->value;
        if (imp->type == IMPLICIT_FORCE_NORMAL && !hasSurface) {
            logError(_logger, "Implicit normal requires at least one surface in the system.");
            return FAILED;
        }
        if (imp->type == IMPLICIT_FORCE_FRICTION && !hasSurfaceFric && !hasBodyWithFriction(body)) {
            logError(_logger, "Implicit friction requires a surface with friction in the system.");
            return FAILED;
        }
    }
    return SUCCEEDED;
}

/*
*  Validations:
*  1. System has at least one body.
*  2. Body names are unique.
*  3. Parent bodies exist.
*  4. No cyclic body dependencies.
*  5. Polygon surfaces are defined by at least 3 points.
*  6. Distance defined between valid bodies.
*  7. Reference frame references valid body.
*  8. Friction coefficients are ≥ 0.
*  9. Force names are unique within a body.
* 10. Directions parallel to surface require at least one surface.
* 11. Implicit normal forces require at least one surface.
* 12. Implicit friction forces require at least one surface with friction.
* 13. Body mass must be positive (> 0).
* 14. Gravity must be non-negative (≥ 0).
* 15. Force magnitudes must be positive (> 0).
* 16. Polar distance magnitudes must be non-negative (≥ 0).
*/
static CompilationStatus validateSystem(System *system) {
    logDebugging(_logger, "Validating system \"%s\"...", system->name);

    if (system->bodies == NULL) {
        logError(_logger, "System \"%s\" has no bodies.", system->name);
        return FAILED;
    }

    BodyTable *table = collectBodyNames(system->bodies);
    if (table == NULL) {
        logError(_logger, "Out of memory.");
        return FAILED;
    }

    CompilationStatus status = SUCCEEDED;

    if (status == SUCCEEDED) { status = validateBodyNames(table, system->bodies); }
    if (status == SUCCEEDED) { status = validateParentBodies(table, system->bodies); }
    if (status == SUCCEEDED) { status = validateNoCyclicBodies(table, system->bodies); }
    if (status == SUCCEEDED) { status = validatePolygonVertices(system->surfaces); }
    if (status == SUCCEEDED) { status = validateDistanceBodies(system->distances, table); }
    if (status == SUCCEEDED) { status = validateDistanceMagnitudes(system->distances); }
    if (status == SUCCEEDED) { status = validateReferenceFrame(system->referenceFrame, table); }
    if (status == SUCCEEDED) { status = validateGravity(system->gravity, system->hasGravity); }
    if (status == SUCCEEDED) {
        for (AstList *s = system->surfaces; s != NULL; s = s->next) {
            Surface *surface = (Surface *) s->value;
            if (status == SUCCEEDED) { status = validateFrictionCoefficients(surface->friction); }
        }
    }

    if (status == SUCCEEDED) {
        for (AstList *b = system->bodies; b != NULL; b = b->next) {
            Body *body = (Body *) b->value;
            if (status == SUCCEEDED) { status = validateMass(body->mass); }
            if (status == SUCCEEDED) { status = validateFrictionCoefficients(body->friction); }
            if (status == SUCCEEDED) { status = validateForceNames(body->forces); }
            if (status == SUCCEEDED) {
                for (AstList *f = body->forces; f != NULL && status == SUCCEEDED; f = f->next) {
                    status = validateForceMagnitude((Force *) f->value);
                }
            }
            if (status == SUCCEEDED) { status = validateSurfaceDependentDirections(body->forces, system->surfaces); }
            if (status == SUCCEEDED) { status = validateImplicitForces(body->implicitForces, system->surfaces, body); }
            if (status != SUCCEEDED) { break; }
        }
    }

    destroyBodyTable(table);
    return status;
}

/* PUBLIC FUNCTIONS */

CompilationStatus executeSemanticAnalysis(Program *program) {
    logDebugging(_logger, "Starting semantic analysis...");
    if (program == NULL) {
        logError(_logger, "AST not found.");
        return FAILED;
    }

    for (AstList *s = program->systems; s != NULL; s = s->next) {
        System *system = (System *) s->value;
        CompilationStatus status = validateSystem(system);
        if (status != SUCCEEDED) {
            logError(_logger, "Semantic analysis FAILED for system \"%s\".", system->name);
            return status;
        }
    }

    logDebugging(_logger, "Semantic analysis SUCCEEDED.");
    return SUCCEEDED;
}
