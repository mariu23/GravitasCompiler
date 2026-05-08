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
ModuleDestructor initializeBisonActionsModule(CompilerState * compilerState);

/**
 * Bison semantic actions.
 */

AstList * AstListSemanticAction(void * value);
AstList * AppendAstListSemanticAction(AstList * list, void * value);

Program * ProgramSemanticAction(AstList * systems);

System * EmptySystemSemanticAction();
System * SystemSemanticAction(char * name, System * system);
System * AddUnitsToSystemSemanticAction(System * system, Units * units);
System * AddGravityToSystemSemanticAction(System * system, const double gravity);
System * AddSurfaceToSystemSemanticAction(System * system, Surface * surface);
System * AddBodyToSystemSemanticAction(System * system, Body * body);
System * AddReferenceFrameToSystemSemanticAction(System * system, ReferenceFrame * referenceFrame);
System * AddDistanceToSystemSemanticAction(System * system, Distance * distance);
System * MergeSystemSemanticAction(System * left, System * right);

Units * EmptyUnitsSemanticAction();
Units * AddMassUnitToUnitsSemanticAction(Units * units);
Units * AddForceUnitToUnitsSemanticAction(Units * units);
Units * AddDistanceUnitToUnitsSemanticAction(Units * units);
Units * MergeUnitsSemanticAction(Units * left, Units * right);

Surface * SurfaceHorizontalSemanticAction(Friction * friction);
Surface * SurfaceInclineSemanticAction(const double angle, Friction * friction);
Friction * FrictionSemanticAction(const double staticCoefficient, const double kineticCoefficient);

Body * EmptyBodyItemsSemanticAction();
Body * EmptyBodySemanticAction(char * name);
Body * BodySemanticAction(char * name, Body * body);
Body * AddBodyTypeToBodySemanticAction(Body * body, const BodyShape shape);
Body * AddMassToBodySemanticAction(Body * body, Mass * mass);
Body * AddForceToBodySemanticAction(Body * body, Force * force);
Body * AddImplicitForcesToBodySemanticAction(Body * body, ImplicitForceList * implicitForces);
Body * MergeBodySemanticAction(Body * left, Body * right);

Mass * MassSemanticAction(const double value, const MassUnit unit);

Force * ForceSemanticAction(char * name, const double magnitude, const ForceUnit unit, Direction * direction);
Direction * AbsoluteDirectionSemanticAction(const double angle);
Direction * ParallelToSurfaceDirectionSemanticAction();

ImplicitForce * ImplicitForceSemanticAction(const ImplicitForceType type);
ImplicitForceList * ImplicitForceListSemanticAction(AstList * forces);

ReferenceFrame * ReferenceFrameAlignedWithSurfaceSemanticAction(char * bodyName);
ReferenceFrame * ReferenceFrameAbsoluteSemanticAction(char * bodyName);

Distance * DistancePolarSemanticAction(
	char * fromBodyName,
	char * toBodyName,
	const double magnitude,
	const DistanceUnit magnitudeUnit,
	const double angle
);
Distance * DistanceCartesianSemanticAction(
	char * fromBodyName,
	char * toBodyName,
	const double x,
	const DistanceUnit xUnit,
	const double y,
	const DistanceUnit yUnit
);

#endif
