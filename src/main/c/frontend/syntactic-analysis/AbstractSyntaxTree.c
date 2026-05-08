#include "AbstractSyntaxTree.h"

/* MODULE INTERNAL STATE */

static Logger * _logger = NULL;

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

static void _destroyBodyValue(void * value);
static void _destroyDistanceValue(void * value);
static void _destroyForceValue(void * value);
static void _destroyImplicitForceValue(void * value);
static void _destroySystemValue(void * value);

static void _logAstDestructor(const char * functionName) {
	logDebugging(_logger, "Executing destructor: %s", functionName);
}

static void _destroyBodyValue(void * value) {
	destroyBody((Body *) value);
}

static void _destroyDistanceValue(void * value) {
	destroyDistance((Distance *) value);
}

static void _destroyForceValue(void * value) {
	destroyForce((Force *) value);
}

static void _destroyImplicitForceValue(void * value) {
	destroyImplicitForce((ImplicitForce *) value);
}

static void _destroySystemValue(void * value) {
	destroySystem((System *) value);
}

/* PUBLIC FUNCTIONS */

AstList * createAstList(void * value) {
	AstList * list = calloc(1, sizeof(AstList));
	if (list != NULL) {
		list->value = value;
	}
	return list;
}

AstList * appendAstList(AstList * list, void * value) {
	AstList * node = createAstList(value);
	if (list == NULL) {
		return node;
	}
	AstList * current = list;
	while (current->next != NULL) {
		current = current->next;
	}
	current->next = node;
	return list;
}

void destroyAstList(AstList * list, AstNodeDestructor destroyValue) {
	_logAstDestructor(__FUNCTION__);
	while (list != NULL) {
		AstList * next = list->next;
		if (destroyValue != NULL) {
			destroyValue(list->value);
		}
		free(list);
		list = next;
	}
}

void destroyBody(Body * body) {
	_logAstDestructor(__FUNCTION__);
	if (body != NULL) {
		free(body->name);
		body->name = NULL;
		destroyMass(body->mass);
		body->mass = NULL;
		destroyAstList(body->forces, _destroyForceValue);
		body->forces = NULL;
		destroyImplicitForceList(body->implicitForces);
		body->implicitForces = NULL;
		free(body);
	}
}

void destroyDirection(Direction * direction) {
	_logAstDestructor(__FUNCTION__);
	if (direction != NULL) {
		free(direction);
	}
}

void destroyDistance(Distance * distance) {
	_logAstDestructor(__FUNCTION__);
	if (distance != NULL) {
		free(distance->fromBodyName);
		distance->fromBodyName = NULL;
		free(distance->toBodyName);
		distance->toBodyName = NULL;
		free(distance);
	}
}

void destroyForce(Force * force) {
	_logAstDestructor(__FUNCTION__);
	if (force != NULL) {
		free(force->name);
		force->name = NULL;
		destroyDirection(force->direction);
		force->direction = NULL;
		free(force);
	}
}

void destroyFriction(Friction * friction) {
	_logAstDestructor(__FUNCTION__);
	if (friction != NULL) {
		free(friction);
	}
}

void destroyImplicitForce(ImplicitForce * implicitForce) {
	_logAstDestructor(__FUNCTION__);
	if (implicitForce != NULL) {
		free(implicitForce);
	}
}

void destroyImplicitForceList(ImplicitForceList * implicitForceList) {
	_logAstDestructor(__FUNCTION__);
	if (implicitForceList != NULL) {
		destroyAstList(implicitForceList->forces, _destroyImplicitForceValue);
		implicitForceList->forces = NULL;
		free(implicitForceList);
	}
}

void destroyMass(Mass * mass) {
	_logAstDestructor(__FUNCTION__);
	if (mass != NULL) {
		free(mass);
	}
}

void destroyProgram(Program * program) {
	_logAstDestructor(__FUNCTION__);
	if (program != NULL) {
		destroyAstList(program->systems, _destroySystemValue);
		program->systems = NULL;
		free(program);
	}
}

void destroyReferenceFrame(ReferenceFrame * referenceFrame) {
	_logAstDestructor(__FUNCTION__);
	if (referenceFrame != NULL) {
		free(referenceFrame->bodyName);
		referenceFrame->bodyName = NULL;
		free(referenceFrame);
	}
}

void destroySurface(Surface * surface) {
	_logAstDestructor(__FUNCTION__);
	if (surface != NULL) {
		destroyFriction(surface->friction);
		surface->friction = NULL;
		free(surface);
	}
}

void destroySystem(System * system) {
	_logAstDestructor(__FUNCTION__);
	if (system != NULL) {
		free(system->name);
		system->name = NULL;
		destroyUnits(system->units);
		system->units = NULL;
		destroySurface(system->surface);
		system->surface = NULL;
		destroyAstList(system->bodies, _destroyBodyValue);
		system->bodies = NULL;
		destroyReferenceFrame(system->referenceFrame);
		system->referenceFrame = NULL;
		destroyAstList(system->distances, _destroyDistanceValue);
		system->distances = NULL;
		free(system);
	}
}

void destroyUnits(Units * units) {
	_logAstDestructor(__FUNCTION__);
	if (units != NULL) {
		free(units);
	}
}
