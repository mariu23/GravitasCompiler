#ifndef ABSTRACT_SYNTAX_TREE_HEADER
#define ABSTRACT_SYNTAX_TREE_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/ModuleDestructor.h"
#include <stdbool.h>
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeAbstractSyntaxTreeModule();

/**
 * Generic AST list used by nodes that can contain repeated children.
 */

typedef struct AstList AstList;
typedef void (*AstNodeDestructor)(void *);

/**
 * Gravitas AST node declarations.
 */

typedef enum BodyShape BodyShape;
typedef enum DirectionType DirectionType;
typedef enum DistanceType DistanceType;
typedef enum ForceUnit ForceUnit;
typedef enum ImplicitForceType ImplicitForceType;
typedef enum MassUnit MassUnit;
typedef enum ReferenceFrameType ReferenceFrameType;
typedef enum SurfaceType SurfaceType;
typedef enum DistanceUnit DistanceUnit;

typedef struct Body Body;
typedef struct Direction Direction;
typedef struct Distance Distance;
typedef struct Force Force;
typedef struct Friction Friction;
typedef struct ImplicitForce ImplicitForce;
typedef struct ImplicitForceList ImplicitForceList;
typedef struct Mass Mass;
typedef struct Program Program;
typedef struct ReferenceFrame ReferenceFrame;
typedef struct Surface Surface;
typedef struct System System;
typedef struct Units Units;

/**
 * Node types for the Abstract Syntax Tree (AST).
 */

enum BodyShape {
	BODY_SHAPE_DEFAULT,
	BODY_SHAPE_BLOCK,
	BODY_SHAPE_SPHERE
};

enum DirectionType {
	DIRECTION_TYPE_ABSOLUTE_ANGLE,
	DIRECTION_TYPE_PARALLEL_TO_SURFACE
};

enum DistanceType {
	DISTANCE_TYPE_POLAR,
	DISTANCE_TYPE_CARTESIAN
};

enum ForceUnit {
	FORCE_UNIT_DEFAULT,
	FORCE_UNIT_NEWTON
};

enum ImplicitForceType {
	IMPLICIT_FORCE_WEIGHT,
	IMPLICIT_FORCE_NORMAL,
	IMPLICIT_FORCE_FRICTION
};

enum MassUnit {
	MASS_UNIT_DEFAULT,
	MASS_UNIT_KG
};

enum ReferenceFrameType {
	REFERENCE_FRAME_ALIGNED_WITH_SURFACE,
	REFERENCE_FRAME_ABSOLUTE
};

enum SurfaceType {
	SURFACE_TYPE_HORIZONTAL,
	SURFACE_TYPE_INCLINE
};

enum DistanceUnit {
	DISTANCE_UNIT_DEFAULT,
	DISTANCE_UNIT_METER
};

struct AstList {
	void * value;
	AstList * next;
};

struct Units {
	bool hasMassUnit;
	bool hasForceUnit;
	bool hasDistanceUnit;
	MassUnit massUnit;
	ForceUnit forceUnit;
	DistanceUnit distanceUnit;
};

struct Friction {
	double staticCoefficient;
	double kineticCoefficient;
};

struct Surface {
	SurfaceType type;
	bool hasAngle;
	double angle;
	Friction * friction;
};

struct Mass {
	double value;
	MassUnit unit;
};

struct Direction {
	DirectionType type;
	double angle;
};

struct Force {
	char * name;
	double magnitude;
	ForceUnit unit;
	Direction * direction;
};

struct ImplicitForce {
	ImplicitForceType type;
};

struct ImplicitForceList {
	AstList * forces;
};

struct Body {
	char * name;
	BodyShape shape;
	Mass * mass;
	AstList * forces;
	ImplicitForceList * implicitForces;
};

struct ReferenceFrame {
	ReferenceFrameType type;
	char * bodyName;
};

struct Distance {
	DistanceType type;
	char * fromBodyName;
	char * toBodyName;
	union {
		struct {
			double magnitude;
			DistanceUnit magnitudeUnit;
			double angle;
		} polar;
		struct {
			double x;
			DistanceUnit xUnit;
			double y;
			DistanceUnit yUnit;
		} cartesian;
	};
};

struct System {
	char * name;
	Units * units;
	bool hasGravity;
	double gravity;
	Surface * surface;
	AstList * bodies;
	ReferenceFrame * referenceFrame;
	AstList * distances;
};

struct Program {
	AstList * systems;
};

/**
 * Generic list helpers.
 */

AstList * createAstList(void * value);
AstList * appendAstList(AstList * list, void * value);
void destroyAstList(AstList * list, AstNodeDestructor destroyValue);

/**
 * Node recursive destructors.
 */

void destroyBody(Body * body);
void destroyDirection(Direction * direction);
void destroyDistance(Distance * distance);
void destroyForce(Force * force);
void destroyFriction(Friction * friction);
void destroyImplicitForce(ImplicitForce * implicitForce);
void destroyImplicitForceList(ImplicitForceList * implicitForceList);
void destroyMass(Mass * mass);
void destroyProgram(Program * program);
void destroyReferenceFrame(ReferenceFrame * referenceFrame);
void destroySurface(Surface * surface);
void destroySystem(System * system);
void destroyUnits(Units * units);

#endif
