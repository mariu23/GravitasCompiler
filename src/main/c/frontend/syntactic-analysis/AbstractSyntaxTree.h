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
typedef enum AngleUnit AngleUnit;
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
typedef struct Point Point;
typedef struct Program Program;
typedef struct ReferenceFrame ReferenceFrame;
typedef struct Surface Surface;
typedef struct System System;
typedef struct Units Units;
typedef struct Value Value;

/**
 * Node types for the Abstract Syntax Tree (AST).
 */

enum BodyShape { BODY_SHAPE_DEFAULT, BODY_SHAPE_BLOCK, BODY_SHAPE_SPHERE };

enum DirectionType { DIRECTION_TYPE_ABSOLUTE_ANGLE, DIRECTION_TYPE_PARALLEL_TO_SURFACE };

enum DistanceType { DISTANCE_TYPE_POLAR, DISTANCE_TYPE_CARTESIAN };

enum AngleUnit { ANGLE_UNIT_DEFAULT, ANGLE_UNIT_DEGREE, ANGLE_UNIT_RADIAN };

enum ForceUnit { FORCE_UNIT_DEFAULT, FORCE_UNIT_NEWTON, FORCE_UNIT_KILONEWTON };

enum ImplicitForceType { IMPLICIT_FORCE_WEIGHT, IMPLICIT_FORCE_NORMAL, IMPLICIT_FORCE_FRICTION };

enum MassUnit { MASS_UNIT_DEFAULT, MASS_UNIT_KG, MASS_UNIT_GRAM, MASS_UNIT_MILLIGRAM };

enum ReferenceFrameType { REFERENCE_FRAME_ALIGNED_WITH_SURFACE, REFERENCE_FRAME_ABSOLUTE };

enum SurfaceType { SURFACE_TYPE_HORIZONTAL, SURFACE_TYPE_INCLINE, SURFACE_TYPE_POLYGON };

enum DistanceUnit {
    DISTANCE_UNIT_DEFAULT,
    DISTANCE_UNIT_METER,
    DISTANCE_UNIT_CENTIMETER,
    DISTANCE_UNIT_MILLIMETER,
    DISTANCE_UNIT_KILOMETER
};

struct Value {
    double numericValue;
    char *sourceText;// for printing
};

struct AstList {
    void *value;
    AstList *next;
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
    Value staticCoefficient;
    Value kineticCoefficient;
};

struct Surface {
    SurfaceType type;
    bool hasAngle;
    Value angle;
    AngleUnit angleUnit;
    Friction *friction;
    AstList *vertices;
};

struct Mass {
    Value value;
    MassUnit unit;
};

struct Direction {
    DirectionType type;
    Value angle;
    AngleUnit angleUnit;
};

struct Force {
    char *name;
    Value magnitude;
    ForceUnit unit;
    Direction *direction;
};

struct ImplicitForce {
    ImplicitForceType type;
    char *name;
};

struct ImplicitForceList {
    AstList *forces;
};

struct Body {
    char *name;
    char *parentBodyName;
    BodyShape shape;
    Mass *mass;
    AstList *forces;
    ImplicitForceList *implicitForces;
    Friction *friction;
};

struct ReferenceFrame {
    ReferenceFrameType type;
    char *bodyName;
};

struct Point {
    Value x;
    DistanceUnit xUnit;
    Value y;
    DistanceUnit yUnit;
};

struct Distance {
    DistanceType type;
    char *fromBodyName;
    char *toBodyName;
    union {
        struct {
            Value magnitude;
            DistanceUnit magnitudeUnit;
            Value angle;
            AngleUnit angleUnit;
        } polar;
        struct {
            Value x;
            DistanceUnit xUnit;
            Value y;
            DistanceUnit yUnit;
        } cartesian;
    };
};

struct System {
    char *name;
    Units *units;
    bool hasGravity;
    Value gravity;
    AstList *surfaces;
    AstList *bodies;
    ReferenceFrame *referenceFrame;
    AstList *distances;
};

struct Program {
    AstList *systems;
};

/**
 * Value helpers.
 */

Value createValue(const double numericValue, const char *sourceText);
Value negateValue(const Value v);
Value multiplyValues(const Value a, const Value b);
Value divideValues(const Value a, const Value b);
Value valueFromParentheses(const Value inner);
void destroyValue(Value value);

/**
 * Generic list helpers.
 */

AstList *createAstList(void *value);
AstList *appendAstList(AstList *list, void *value);
void destroyAstList(AstList *list, AstNodeDestructor destroyValue);

/**
 * Node recursive destructors.
 */

void destroyBody(Body *body);
void destroyDirection(Direction *direction);
void destroyDistance(Distance *distance);
void destroyForce(Force *force);
void destroyFriction(Friction *friction);
void destroyImplicitForce(ImplicitForce *implicitForce);
void destroyImplicitForceList(ImplicitForceList *implicitForceList);
void destroyMass(Mass *mass);
void destroyPoint(Point *point);
void destroyProgram(Program *program);
void destroyReferenceFrame(ReferenceFrame *referenceFrame);
void destroySurface(Surface *surface);
void destroySystem(System *system);
void destroyUnits(Units *units);

#endif
