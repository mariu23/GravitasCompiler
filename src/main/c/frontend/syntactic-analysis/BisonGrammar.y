%{

#include "../../support/type/TokenLabel.h"
#include "AbstractSyntaxTree.h"
#include "BisonActions.h"
#include <stdlib.h>

/**
 * The error reporting function for Bison parser.
 *
 * @todo Add location to the grammar and "pushToken" API function.
 *
 * @see https://www.gnu.org/software/bison/manual/html_node/Error-Reporting-Function.html
 * @see https://www.gnu.org/software/bison/manual/html_node/Tracking-Locations.html
 */
void yyerror(const YYLTYPE * location, const char * message) {}

%}

%code requires {
	#include "../../support/type/TokenLabel.h"
	#include "AbstractSyntaxTree.h"

	typedef struct {
		double value;
		ForceUnit unit;
	} ForceMagnitudeSpec;

	typedef struct {
		double value;
		AngleUnit unit;
	} AngleSpec;

	typedef struct {
		double magnitude;
		DistanceUnit unit;
		AngleSpec angle;
	} PolarDistanceSpec;

	typedef struct {
		double x;
		DistanceUnit xUnit;
		double y;
		DistanceUnit yUnit;
	} CartesianDistanceSpec;
}

// You touch this, and you die.
%define api.pure full
%define api.push-pull push
%define api.value.union.name SemanticValue
%define parse.error detailed
%locations

%union {
	/** Terminals. */

	double number;
	char * string;
	TokenLabel token;

	/** Non-terminals. */

	AstList * astList;
	AngleSpec angleSpec;
	Body * body;
	BodyShape bodyShape;
	CartesianDistanceSpec cartesianDistanceSpec;
	Direction * direction;
	Distance * distance;
	DistanceUnit distanceUnit;
	Force * force;
	ForceMagnitudeSpec forceMagnitudeSpec;
	ForceUnit forceUnit;
	Friction * friction;
	ImplicitForce * implicitForce;
	ImplicitForceList * implicitForceList;
	Mass * mass;
	MassUnit massUnit;
	PolarDistanceSpec polarDistanceSpec;
	Program * program;
	ReferenceFrame * referenceFrame;
	Surface * surface;
	System * system;
	Units * units;
}

%destructor { free($$); } <string>
%destructor { destroySystem($$); } <system>
%destructor { destroyUnits($$); } <units>
%destructor { destroySurface($$); } <surface>
%destructor { destroyFriction($$); } <friction>
%destructor { destroyBody($$); } <body>
%destructor { destroyMass($$); } <mass>
%destructor { destroyForce($$); } <force>
%destructor { destroyDirection($$); } <direction>
%destructor { destroyImplicitForce($$); } <implicitForce>
%destructor { destroyImplicitForceList($$); } <implicitForceList>
%destructor { destroyReferenceFrame($$); } <referenceFrame>
%destructor { destroyDistance($$); } <distance>

/** Terminals. */
%token <string> ID
%token <number> NUMBER

%token ABSOLUTE
%token ALIGNED
%token ANGLE
%token BLOCK
%token BODY
%token CLOSE_BRACE
%token CLOSE_COMMENT
%token COMMA
%token DEGREE
%token DIRECTION
%token DISTANCE
%token FORCE
%token FRAME
%token FRICTION
%token GRAM
%token GRAVITY
%token HORIZONTAL
%token IMPLICIT
%token INCLINE
%token KG
%token KILOMETER
%token KILONEWTON
%token KINETIC
%token MAGNITUDE
%token MASS
%token METER
%token MILLIGRAM
%token MILLIMETER
%token CENTIMETER
%token NEWTON
%token NORMAL
%token ON
%token OPEN_BRACE
%token OPEN_COMMENT
%token PARALLEL
%token REFERENCE
%token RADIAN
%token SEMICOLON
%token SPHERE
%token STATIC
%token SURFACE
%token SYSTEM
%token TO
%token TYPE
%token UNITS
%token WEIGHT
%token WITH
%token X_AXIS
%token Y_AXIS

%token IGNORED
%token UNKNOWN

/** Non-terminals. */
%type <program> program
%type <astList> systemList implicitForceList
%type <system> system systemItemsWithBody systemItemsBeforeFirstBody systemItems systemItem nonBodySystemItem
%type <units> unitsDeclaration unitDeclarationList unitDeclaration
%type <surface> surfaceDeclaration
%type <friction> optionalFriction frictionDeclaration
%type <body> bodyDeclaration bodyItems bodyItem
%type <bodyShape> bodyTypeDeclaration
%type <mass> massDeclaration
%type <massUnit> optionalMassUnit
%type <force> forceDeclaration
%type <forceMagnitudeSpec> magnitudeDeclaration
%type <forceUnit> optionalForceUnit
%type <direction> directionDeclaration directionSpec
%type <implicitForceList> implicitForcesDeclaration
%type <implicitForce> implicitForce
%type <referenceFrame> referenceFrameDeclaration
%type <distance> distanceDeclaration
%type <polarDistanceSpec> polarDistanceSpec
%type <cartesianDistanceSpec> cartesianDistanceSpec
%type <distanceUnit> optionalDistanceUnit
%type <number> gravityDeclaration
%type <angleSpec> angleValue

%%

// IMPORTANT: To use lambda in the following grammar, use the %empty symbol.

program:
	systemList												{ $$ = ProgramSemanticAction($1); }
	;

systemList:
	system													{ $$ = AstListSemanticAction($1); }
	| systemList system										{ $$ = AppendAstListSemanticAction($1, $2); }
	;

system:
	SYSTEM ID OPEN_BRACE systemItemsWithBody CLOSE_BRACE	{ $$ = SystemSemanticAction($2, $4); }
	;

systemItemsWithBody:
	systemItemsBeforeFirstBody bodyDeclaration systemItems	{ $$ = MergeSystemSemanticAction(AddBodyToSystemSemanticAction($1, $2), $3); }
	;

systemItemsBeforeFirstBody:
	%empty													{ $$ = EmptySystemSemanticAction(); }
	| systemItemsBeforeFirstBody nonBodySystemItem			{ $$ = MergeSystemSemanticAction($1, $2); }
	;

systemItems:
	%empty													{ $$ = EmptySystemSemanticAction(); }
	| systemItems systemItem								{ $$ = MergeSystemSemanticAction($1, $2); }
	;

systemItem:
	nonBodySystemItem										{ $$ = $1; }
	| bodyDeclaration										{ $$ = AddBodyToSystemSemanticAction(EmptySystemSemanticAction(), $1); }
	;

nonBodySystemItem:
	unitsDeclaration										{ $$ = AddUnitsToSystemSemanticAction(EmptySystemSemanticAction(), $1); }
	| gravityDeclaration									{ $$ = AddGravityToSystemSemanticAction(EmptySystemSemanticAction(), $1); }
	| surfaceDeclaration									{ $$ = AddSurfaceToSystemSemanticAction(EmptySystemSemanticAction(), $1); }
	| referenceFrameDeclaration								{ $$ = AddReferenceFrameToSystemSemanticAction(EmptySystemSemanticAction(), $1); }
	| distanceDeclaration									{ $$ = AddDistanceToSystemSemanticAction(EmptySystemSemanticAction(), $1); }
	;

unitsDeclaration:
	UNITS OPEN_BRACE unitDeclarationList CLOSE_BRACE		{ $$ = $3; }
	;

unitDeclarationList:
	unitDeclaration											{ $$ = $1; }
	| unitDeclarationList unitDeclaration					{ $$ = MergeUnitsSemanticAction($1, $2); }
	;

unitDeclaration:
	MASS KG SEMICOLON										{ $$ = AddMassUnitToUnitsSemanticAction(EmptyUnitsSemanticAction(), MASS_UNIT_KG); }
	| MASS GRAM SEMICOLON									{ $$ = AddMassUnitToUnitsSemanticAction(EmptyUnitsSemanticAction(), MASS_UNIT_GRAM); }
	| MASS MILLIGRAM SEMICOLON								{ $$ = AddMassUnitToUnitsSemanticAction(EmptyUnitsSemanticAction(), MASS_UNIT_MILLIGRAM); }
	| FORCE NEWTON SEMICOLON								{ $$ = AddForceUnitToUnitsSemanticAction(EmptyUnitsSemanticAction(), FORCE_UNIT_NEWTON); }
	| FORCE KILONEWTON SEMICOLON							{ $$ = AddForceUnitToUnitsSemanticAction(EmptyUnitsSemanticAction(), FORCE_UNIT_KILONEWTON); }
	| DISTANCE METER SEMICOLON								{ $$ = AddDistanceUnitToUnitsSemanticAction(EmptyUnitsSemanticAction(), DISTANCE_UNIT_METER); }
	| DISTANCE CENTIMETER SEMICOLON							{ $$ = AddDistanceUnitToUnitsSemanticAction(EmptyUnitsSemanticAction(), DISTANCE_UNIT_CENTIMETER); }
	| DISTANCE MILLIMETER SEMICOLON							{ $$ = AddDistanceUnitToUnitsSemanticAction(EmptyUnitsSemanticAction(), DISTANCE_UNIT_MILLIMETER); }
	| DISTANCE KILOMETER SEMICOLON							{ $$ = AddDistanceUnitToUnitsSemanticAction(EmptyUnitsSemanticAction(), DISTANCE_UNIT_KILOMETER); }
	;

gravityDeclaration:
	GRAVITY NUMBER SEMICOLON								{ $$ = $2; }
	;

surfaceDeclaration:
	SURFACE OPEN_BRACE TYPE HORIZONTAL SEMICOLON optionalFriction CLOSE_BRACE
															{ $$ = SurfaceHorizontalSemanticAction($6); }
	| SURFACE OPEN_BRACE TYPE INCLINE SEMICOLON ANGLE angleValue SEMICOLON optionalFriction CLOSE_BRACE
															{ $$ = SurfaceInclineSemanticAction($7.value, $7.unit, $9); }
	;

optionalFriction:
	%empty													{ $$ = NULL; }
	| frictionDeclaration									{ $$ = $1; }
	;

frictionDeclaration:
	FRICTION OPEN_BRACE STATIC NUMBER SEMICOLON KINETIC NUMBER SEMICOLON CLOSE_BRACE
															{ $$ = FrictionSemanticAction($4, $7); }
	;

referenceFrameDeclaration:
	REFERENCE FRAME ALIGNED WITH SURFACE ON ID SEMICOLON	{ $$ = ReferenceFrameAlignedWithSurfaceSemanticAction($7); }
	| REFERENCE FRAME ABSOLUTE ON ID SEMICOLON				{ $$ = ReferenceFrameAbsoluteSemanticAction($5); }
	;

distanceDeclaration:
	DISTANCE ID ID OPEN_BRACE polarDistanceSpec CLOSE_BRACE
															{ $$ = DistancePolarSemanticAction($2, $3, $5.magnitude, $5.unit, $5.angle.value, $5.angle.unit); }
	| DISTANCE ID ID OPEN_BRACE cartesianDistanceSpec CLOSE_BRACE
															{ $$ = DistanceCartesianSemanticAction($2, $3, $5.x, $5.xUnit, $5.y, $5.yUnit); }
	;

polarDistanceSpec:
	MAGNITUDE NUMBER optionalDistanceUnit SEMICOLON ANGLE angleValue SEMICOLON
															{ $$.magnitude = $2; $$.unit = $3; $$.angle = $6; }
	;

cartesianDistanceSpec:
	X_AXIS NUMBER optionalDistanceUnit SEMICOLON Y_AXIS NUMBER optionalDistanceUnit SEMICOLON
															{ $$.x = $2; $$.xUnit = $3; $$.y = $6; $$.yUnit = $7; }
	;

optionalDistanceUnit:
	%empty													{ $$ = DISTANCE_UNIT_DEFAULT; }
	| METER													{ $$ = DISTANCE_UNIT_METER; }
	| CENTIMETER											{ $$ = DISTANCE_UNIT_CENTIMETER; }
	| MILLIMETER											{ $$ = DISTANCE_UNIT_MILLIMETER; }
	| KILOMETER												{ $$ = DISTANCE_UNIT_KILOMETER; }
	;

angleValue:
	NUMBER DEGREE											{ $$.value = $1; $$.unit = ANGLE_UNIT_DEGREE; }
	| NUMBER RADIAN											{ $$.value = $1; $$.unit = ANGLE_UNIT_RADIAN; }
	;

bodyDeclaration:
	BODY ID SEMICOLON										{ $$ = EmptyBodySemanticAction($2); }
	| BODY ID OPEN_BRACE bodyItems CLOSE_BRACE				{ $$ = BodySemanticAction($2, $4); }
	;

bodyItems:
	%empty													{ $$ = EmptyBodyItemsSemanticAction(); }
	| bodyItems bodyItem									{ $$ = MergeBodySemanticAction($1, $2); }
	;

bodyItem:
	bodyTypeDeclaration										{ $$ = AddBodyTypeToBodySemanticAction(EmptyBodyItemsSemanticAction(), $1); }
	| massDeclaration										{ $$ = AddMassToBodySemanticAction(EmptyBodyItemsSemanticAction(), $1); }
	| forceDeclaration										{ $$ = AddForceToBodySemanticAction(EmptyBodyItemsSemanticAction(), $1); }
	| implicitForcesDeclaration								{ $$ = AddImplicitForcesToBodySemanticAction(EmptyBodyItemsSemanticAction(), $1); }
	;

bodyTypeDeclaration:
	TYPE BLOCK SEMICOLON									{ $$ = BODY_SHAPE_BLOCK; }
	| TYPE SPHERE SEMICOLON									{ $$ = BODY_SHAPE_SPHERE; }
	;

massDeclaration:
	MASS NUMBER optionalMassUnit SEMICOLON					{ $$ = MassSemanticAction($2, $3); }
	;

optionalMassUnit:
	%empty													{ $$ = MASS_UNIT_DEFAULT; }
	| KG													{ $$ = MASS_UNIT_KG; }
	| GRAM													{ $$ = MASS_UNIT_GRAM; }
	| MILLIGRAM												{ $$ = MASS_UNIT_MILLIGRAM; }
	;

forceDeclaration:
	FORCE ID OPEN_BRACE magnitudeDeclaration directionDeclaration CLOSE_BRACE
															{ $$ = ForceSemanticAction($2, $4.value, $4.unit, $5); }
	;

magnitudeDeclaration:
	MAGNITUDE NUMBER optionalForceUnit SEMICOLON			{ $$.value = $2; $$.unit = $3; }
	;

optionalForceUnit:
	%empty													{ $$ = FORCE_UNIT_DEFAULT; }
	| NEWTON												{ $$ = FORCE_UNIT_NEWTON; }
	| KILONEWTON											{ $$ = FORCE_UNIT_KILONEWTON; }
	;

directionDeclaration:
	DIRECTION directionSpec SEMICOLON						{ $$ = $2; }
	;

directionSpec:
	ANGLE angleValue										{ $$ = AbsoluteDirectionSemanticAction($2.value, $2.unit); }
	| PARALLEL TO SURFACE									{ $$ = ParallelToSurfaceDirectionSemanticAction(); }
	;

implicitForcesDeclaration:
	IMPLICIT implicitForceList SEMICOLON					{ $$ = ImplicitForceListSemanticAction($2); }
	;

implicitForceList:
	implicitForce											{ $$ = AstListSemanticAction($1); }
	| implicitForceList COMMA implicitForce					{ $$ = AppendAstListSemanticAction($1, $3); }
	;

implicitForce:
	WEIGHT													{ $$ = ImplicitForceSemanticAction(IMPLICIT_FORCE_WEIGHT); }
	| NORMAL												{ $$ = ImplicitForceSemanticAction(IMPLICIT_FORCE_NORMAL); }
	| FRICTION												{ $$ = ImplicitForceSemanticAction(IMPLICIT_FORCE_FRICTION); }
	;

%%
