#ifndef BISON_ACTIONS_HEADER
#define BISON_ACTIONS_HEADER

#include "../../support/logging/Logger.h"
#include "../../support/type/CompilerState.h"
#include "../../support/type/ModuleDestructor.h"
#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonParser.h"
#include <stdlib.h>

/** Initialize module's internal state. */
ModuleDestructor initializeBisonActionsModule(CompilerState *compilerState);

/**
 * Bison semantic actions.
 */

AstList *AstListSemanticAction(void *value);
AstList *AppendAstListSemanticAction(AstList *list, void *value);

Program *ProgramSemanticAction(AstList *systems);

System *EmptySystemSemanticAction();
System *SystemSemanticAction(char *name, System *system);
System *AddUnitsToSystemSemanticAction(System *system, Units *units);
System *AddGravityToSystemSemanticAction(System *system, const Value gravity);
System *AddSurfaceToSystemSemanticAction(System *system, Surface *surface);
System *AddBodyToSystemSemanticAction(System *system, Body *body);
System *AddReferenceFrameToSystemSemanticAction(System *system, ReferenceFrame *referenceFrame);
System *AddDistanceToSystemSemanticAction(System *system, Distance *distance);
System *MergeSystemSemanticAction(System *left, System *right);

Units *EmptyUnitsSemanticAction();
Units *AddMassUnitToUnitsSemanticAction(Units *units, const MassUnit massUnit);
Units *AddForceUnitToUnitsSemanticAction(Units *units, const ForceUnit forceUnit);
Units *AddDistanceUnitToUnitsSemanticAction(Units *units, const DistanceUnit distanceUnit);
Units *MergeUnitsSemanticAction(Units *left, Units *right);

Surface *SurfaceHorizontalSemanticAction(Friction *friction);
Surface *SurfaceInclineSemanticAction(const Value angle, const AngleUnit angleUnit, Friction *friction);
Surface *SurfacePolygonSemanticAction(AstList *vertices, Friction *friction);
Point *PointSemanticAction(const Value x, const DistanceUnit xUnit, const Value y, const DistanceUnit yUnit);
Friction *FrictionSemanticAction(const Value staticCoefficient, const Value kineticCoefficient);

Body *EmptyBodyItemsSemanticAction();
Body *EmptyBodySemanticAction(char *name);
Body *BodySemanticAction(char *name, Body *body);
Body *BodyOnTopOfBodySemanticAction(char *name, char *parentBodyName, Body *body);
Body *AddBodyTypeToBodySemanticAction(Body *body, const BodyShape shape);
Body *AddMassToBodySemanticAction(Body *body, Mass *mass);
Body *AddForceToBodySemanticAction(Body *body, Force *force);
Body *AddImplicitForcesToBodySemanticAction(Body *body, ImplicitForceList *implicitForces);
Body *AddFrictionToBodySemanticAction(Body *body, Friction *friction);
Body *MergeBodySemanticAction(Body *left, Body *right);

Mass *MassSemanticAction(const Value value, const MassUnit unit);

Force *ForceSemanticAction(char *name, const Value magnitude, const ForceUnit unit, Direction *direction);
Direction *AbsoluteDirectionSemanticAction(const Value angle, const AngleUnit angleUnit);
Direction *ParallelToSurfaceDirectionSemanticAction();

ImplicitForce *ImplicitForceSemanticAction(const ImplicitForceType type);
ImplicitForce *ImplicitForceWithNameSemanticAction(const ImplicitForceType type, char *name);
ImplicitForceList *ImplicitForceListSemanticAction(AstList *forces);

ReferenceFrame *ReferenceFrameAlignedWithSurfaceSemanticAction(char *bodyName);
ReferenceFrame *ReferenceFrameAbsoluteSemanticAction(char *bodyName);

Distance *DistancePolarSemanticAction(char *fromBodyName, char *toBodyName, const Value magnitude,
                                      const DistanceUnit magnitudeUnit, const Value angle, const AngleUnit angleUnit);
Distance *DistanceCartesianSemanticAction(char *fromBodyName, char *toBodyName, const Value x, const DistanceUnit xUnit,
                                          const Value y, const DistanceUnit yUnit);

#endif
