#include "BisonActions.h"

/* MODULE INTERNAL STATE */

static CompilerState * _compilerState = NULL;
static Logger * _logger = NULL;

/** Shutdown module's internal state. */
void _shutdownBisonActionsModule() {
	if (_logger != NULL) {
		logDebugging(_logger, "Destroying module: BisonActions...");
		destroyLogger(_logger);
		_logger = NULL;
	}
	_compilerState = NULL;
}

ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState) {
	_compilerState = compilerState;
	_logger = createLogger("BisonActions");
	return _shutdownBisonActionsModule;
}

/* PRIVATE FUNCTIONS */

static void _logSyntacticAnalyzerAction(const char * functionName);
static void _destroyImplicitForceNodeOnly(AstList * list);
static AstList * _mergeAstLists(AstList * target, AstList * source);

/**
 * Logs a syntactic-analyzer action in DEBUGGING level.
 */
static void _logSyntacticAnalyzerAction(const char * functionName) {
	logDebugging(_logger, "%s", functionName);
}

static void _destroyImplicitForceNodeOnly(AstList * list) {
	while (list != NULL) {
		AstList * next = list->next;
		free(list);
		list = next;
	}
}

static AstList * _mergeAstLists(AstList * target, AstList * source) {
	AstList * current = source;
	while (current != NULL) {
		target = appendAstList(target, current->value);
		current = current->next;
	}
	_destroyImplicitForceNodeOnly(source);
	return target;
}

/* PUBLIC FUNCTIONS */

AstList * AstListSemanticAction(void * value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return createAstList(value);
}

AstList * AppendAstListSemanticAction(AstList * list, void * value) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return appendAstList(list, value);
}

Program * ProgramSemanticAction(AstList * systems) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Program * program = calloc(1, sizeof(Program));
	program->systems = systems;
	_compilerState->abstractSyntaxtTree = program;
	return program;
}

System * EmptySystemSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(System));
}

System * SystemSemanticAction(char * name, System * system) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (system == NULL) {
		system = EmptySystemSemanticAction();
	}
	system->name = name;
	return system;
}

System * AddUnitsToSystemSemanticAction(System * system, Units * units) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (system == NULL) {
		system = EmptySystemSemanticAction();
	}
	destroyUnits(system->units);
	system->units = units;
	return system;
}

System * AddGravityToSystemSemanticAction(System * system, const Value gravity) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (system == NULL) {
		system = EmptySystemSemanticAction();
	}
	system->hasGravity = true;
	system->gravity = gravity;
	return system;
}

System * AddSurfaceToSystemSemanticAction(System * system, Surface * surface) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (system == NULL) {
		system = EmptySystemSemanticAction();
	}
	system->surfaces = appendAstList(system->surfaces, surface);
	return system;
}

System * AddBodyToSystemSemanticAction(System * system, Body * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (system == NULL) {
		system = EmptySystemSemanticAction();
	}
	system->bodies = appendAstList(system->bodies, body);
	return system;
}

System * AddReferenceFrameToSystemSemanticAction(System * system, ReferenceFrame * referenceFrame) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (system == NULL) {
		system = EmptySystemSemanticAction();
	}
	destroyReferenceFrame(system->referenceFrame);
	system->referenceFrame = referenceFrame;
	return system;
}

System * AddDistanceToSystemSemanticAction(System * system, Distance * distance) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (system == NULL) {
		system = EmptySystemSemanticAction();
	}
	system->distances = appendAstList(system->distances, distance);
	return system;
}

System * MergeSystemSemanticAction(System * left, System * right) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (left == NULL) {
		return right;
	}
	if (right == NULL) {
		return left;
	}
	if (right->units != NULL) {
		destroyUnits(left->units);
		left->units = right->units;
		right->units = NULL;
	}
	if (right->hasGravity) {
		left->hasGravity = true;
		left->gravity = right->gravity;
	}
	if (right->surfaces != NULL) {
		left->surfaces = _mergeAstLists(left->surfaces, right->surfaces);
		right->surfaces = NULL;
	}
	left->bodies = _mergeAstLists(left->bodies, right->bodies);
	right->bodies = NULL;
	if (right->referenceFrame != NULL) {
		destroyReferenceFrame(left->referenceFrame);
		left->referenceFrame = right->referenceFrame;
		right->referenceFrame = NULL;
	}
	left->distances = _mergeAstLists(left->distances, right->distances);
	right->distances = NULL;
	free(right->name);
	right->name = NULL;
	free(right);
	return left;
}

Units * EmptyUnitsSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	return calloc(1, sizeof(Units));
}

Units * AddMassUnitToUnitsSemanticAction(Units * units, const MassUnit massUnit) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (units == NULL) {
		units = EmptyUnitsSemanticAction();
	}
	units->hasMassUnit = true;
	units->massUnit = massUnit;
	return units;
}

Units * AddForceUnitToUnitsSemanticAction(Units * units, const ForceUnit forceUnit) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (units == NULL) {
		units = EmptyUnitsSemanticAction();
	}
	units->hasForceUnit = true;
	units->forceUnit = forceUnit;
	return units;
}

Units * AddDistanceUnitToUnitsSemanticAction(Units * units, const DistanceUnit distanceUnit) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (units == NULL) {
		units = EmptyUnitsSemanticAction();
	}
	units->hasDistanceUnit = true;
	units->distanceUnit = distanceUnit;
	return units;
}

Units * MergeUnitsSemanticAction(Units * left, Units * right) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (left == NULL) {
		return right;
	}
	if (right == NULL) {
		return left;
	}
	if (right->hasMassUnit) {
		left->hasMassUnit = true;
		left->massUnit = right->massUnit;
	}
	if (right->hasForceUnit) {
		left->hasForceUnit = true;
		left->forceUnit = right->forceUnit;
	}
	if (right->hasDistanceUnit) {
		left->hasDistanceUnit = true;
		left->distanceUnit = right->distanceUnit;
	}
	free(right);
	return left;
}

Surface * SurfaceHorizontalSemanticAction(Friction * friction) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Surface * surface = calloc(1, sizeof(Surface));
	surface->type = SURFACE_TYPE_HORIZONTAL;
	surface->friction = friction;
	return surface;
}

Surface * SurfaceInclineSemanticAction(const Value angle, const AngleUnit angleUnit, Friction * friction) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Surface * surface = calloc(1, sizeof(Surface));
	surface->type = SURFACE_TYPE_INCLINE;
	surface->hasAngle = true;
	surface->angle = angle;
	surface->angleUnit = angleUnit;
	surface->friction = friction;
	return surface;
}

Surface * SurfacePolygonSemanticAction(AstList * vertices, Friction * friction) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Surface * surface = calloc(1, sizeof(Surface));
	surface->type = SURFACE_TYPE_POLYGON;
	surface->vertices = vertices;
	surface->friction = friction;
	return surface;
}

Point * PointSemanticAction(const Value x, const DistanceUnit xUnit, const Value y, const DistanceUnit yUnit) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Point * point = calloc(1, sizeof(Point));
	point->x = x;
	point->xUnit = xUnit;
	point->y = y;
	point->yUnit = yUnit;
	return point;
}

Friction * FrictionSemanticAction(const Value staticCoefficient, const Value kineticCoefficient) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Friction * friction = calloc(1, sizeof(Friction));
	friction->staticCoefficient = staticCoefficient;
	friction->kineticCoefficient = kineticCoefficient;
	return friction;
}

Body * EmptyBodyItemsSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Body * body = calloc(1, sizeof(Body));
	body->shape = BODY_SHAPE_DEFAULT;
	return body;
}

Body * EmptyBodySemanticAction(char * name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Body * body = EmptyBodyItemsSemanticAction();
	body->name = name;
	return body;
}

Body * BodySemanticAction(char * name, Body * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (body == NULL) {
		body = EmptyBodyItemsSemanticAction();
	}
	body->name = name;
	return body;
}

Body * BodyOnTopOfBodySemanticAction(char * name, char * parentBodyName, Body * body) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (body == NULL) {
		body = EmptyBodyItemsSemanticAction();
	}
	body->name = name;
	body->parentBodyName = parentBodyName;
	return body;
}

Body * AddBodyTypeToBodySemanticAction(Body * body, const BodyShape shape) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (body == NULL) {
		body = EmptyBodyItemsSemanticAction();
	}
	body->shape = shape;
	return body;
}

Body * AddMassToBodySemanticAction(Body * body, Mass * mass) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (body == NULL) {
		body = EmptyBodyItemsSemanticAction();
	}
	destroyMass(body->mass);
	body->mass = mass;
	return body;
}

Body * AddForceToBodySemanticAction(Body * body, Force * force) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (body == NULL) {
		body = EmptyBodyItemsSemanticAction();
	}
	body->forces = appendAstList(body->forces, force);
	return body;
}

Body * AddImplicitForcesToBodySemanticAction(Body * body, ImplicitForceList * implicitForces) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (body == NULL) {
		body = EmptyBodyItemsSemanticAction();
	}
	if (body->implicitForces == NULL) {
		body->implicitForces = implicitForces;
	}
	else if (implicitForces != NULL) {
		body->implicitForces->forces = _mergeAstLists(body->implicitForces->forces, implicitForces->forces);
		implicitForces->forces = NULL;
		free(implicitForces);
	}
	return body;
}

Body * AddFrictionToBodySemanticAction(Body * body, Friction * friction) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (body == NULL) {
		body = EmptyBodyItemsSemanticAction();
	}
	destroyFriction(body->friction);
	body->friction = friction;
	return body;
}

Body * MergeBodySemanticAction(Body * left, Body * right) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	if (left == NULL) {
		return right;
	}
	if (right == NULL) {
		return left;
	}
	if (right->shape != BODY_SHAPE_DEFAULT) {
		left->shape = right->shape;
	}
	if (right->mass != NULL) {
		destroyMass(left->mass);
		left->mass = right->mass;
		right->mass = NULL;
	}
	left->forces = _mergeAstLists(left->forces, right->forces);
	right->forces = NULL;
	if (right->implicitForces != NULL) {
		left = AddImplicitForcesToBodySemanticAction(left, right->implicitForces);
		right->implicitForces = NULL;
	}
	if (right->friction != NULL) {
		destroyFriction(left->friction);
		left->friction = right->friction;
		right->friction = NULL;
	}
	free(right->name);
	right->name = NULL;
	free(right);
	return left;
}

Mass * MassSemanticAction(const Value value, const MassUnit unit) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Mass * mass = calloc(1, sizeof(Mass));
	mass->value = value;
	mass->unit = unit;
	return mass;
}

Force * ForceSemanticAction(char * name, const Value magnitude, const ForceUnit unit, Direction * direction) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Force * force = calloc(1, sizeof(Force));
	force->name = name;
	force->magnitude = magnitude;
	force->unit = unit;
	force->direction = direction;
	return force;
}

Direction * AbsoluteDirectionSemanticAction(const Value angle, const AngleUnit angleUnit) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Direction * direction = calloc(1, sizeof(Direction));
	direction->type = DIRECTION_TYPE_ABSOLUTE_ANGLE;
	direction->angle = angle;
	direction->angleUnit = angleUnit;
	return direction;
}

Direction * ParallelToSurfaceDirectionSemanticAction() {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Direction * direction = calloc(1, sizeof(Direction));
	direction->type = DIRECTION_TYPE_PARALLEL_TO_SURFACE;
	return direction;
}

ImplicitForce * ImplicitForceSemanticAction(const ImplicitForceType type) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ImplicitForce * implicitForce = calloc(1, sizeof(ImplicitForce));
	implicitForce->type = type;
	return implicitForce;
}

ImplicitForce * ImplicitForceWithNameSemanticAction(const ImplicitForceType type, char * name) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ImplicitForce * implicitForce = calloc(1, sizeof(ImplicitForce));
	implicitForce->type = type;
	implicitForce->name = name;
	return implicitForce;
}

ImplicitForceList * ImplicitForceListSemanticAction(AstList * forces) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ImplicitForceList * implicitForceList = calloc(1, sizeof(ImplicitForceList));
	implicitForceList->forces = forces;
	return implicitForceList;
}

ReferenceFrame * ReferenceFrameAlignedWithSurfaceSemanticAction(char * bodyName) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ReferenceFrame * referenceFrame = calloc(1, sizeof(ReferenceFrame));
	referenceFrame->type = REFERENCE_FRAME_ALIGNED_WITH_SURFACE;
	referenceFrame->bodyName = bodyName;
	return referenceFrame;
}

ReferenceFrame * ReferenceFrameAbsoluteSemanticAction(char * bodyName) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	ReferenceFrame * referenceFrame = calloc(1, sizeof(ReferenceFrame));
	referenceFrame->type = REFERENCE_FRAME_ABSOLUTE;
	referenceFrame->bodyName = bodyName;
	return referenceFrame;
}

Distance * DistancePolarSemanticAction(
	char * fromBodyName,
	char * toBodyName,
	const Value magnitude,
	const DistanceUnit magnitudeUnit,
	const Value angle,
	const AngleUnit angleUnit
) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Distance * distance = calloc(1, sizeof(Distance));
	distance->type = DISTANCE_TYPE_POLAR;
	distance->fromBodyName = fromBodyName;
	distance->toBodyName = toBodyName;
	distance->polar.magnitude = magnitude;
	distance->polar.magnitudeUnit = magnitudeUnit;
	distance->polar.angle = angle;
	distance->polar.angleUnit = angleUnit;
	return distance;
}

Distance * DistanceCartesianSemanticAction(
	char * fromBodyName,
	char * toBodyName,
	const Value x,
	const DistanceUnit xUnit,
	const Value y,
	const DistanceUnit yUnit
) {
	_logSyntacticAnalyzerAction(__FUNCTION__);
	Distance * distance = calloc(1, sizeof(Distance));
	distance->type = DISTANCE_TYPE_CARTESIAN;
	distance->fromBodyName = fromBodyName;
	distance->toBodyName = toBodyName;
	distance->cartesian.x = x;
	distance->cartesian.xUnit = xUnit;
	distance->cartesian.y = y;
	distance->cartesian.yUnit = yUnit;
	return distance;
}
