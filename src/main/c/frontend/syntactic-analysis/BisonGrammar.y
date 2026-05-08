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

	Program * program;
}

%destructor { free($$); } <string>

/** Terminals. */
%token <string> ID
%token <number> NUMBER

%token <token> ABSOLUTE
%token <token> ALIGNED
%token <token> ANGLE
%token <token> BLOCK
%token <token> BODY
%token <token> CLOSE_BRACE
%token <token> CLOSE_COMMENT
%token <token> COMMA
%token <token> DEGREE
%token <token> DIRECTION
%token <token> DISTANCE
%token <token> FORCE
%token <token> FRAME
%token <token> FRICTION
%token <token> GRAVITY
%token <token> HORIZONTAL
%token <token> IMPLICIT
%token <token> INCLINE
%token <token> KG
%token <token> KINETIC
%token <token> MAGNITUDE
%token <token> MASS
%token <token> METER
%token <token> NEWTON
%token <token> NORMAL
%token <token> ON
%token <token> OPEN_BRACE
%token <token> OPEN_COMMENT
%token <token> PARALLEL
%token <token> REFERENCE
%token <token> SEMICOLON
%token <token> SPHERE
%token <token> STATIC
%token <token> SURFACE
%token <token> SYSTEM
%token <token> TO
%token <token> TYPE
%token <token> UNITS
%token <token> WEIGHT
%token <token> WITH
%token <token> X_AXIS
%token <token> Y_AXIS

%token <token> IGNORED
%token <token> UNKNOWN

/** Non-terminals. */
%type <program> program

%%

// IMPORTANT: To use lambda in the following grammar, use the %empty symbol.

program:
	systemList												{ $$ = NULL; }
	;

systemList:
	system
	| systemList system
	;

system:
	SYSTEM ID OPEN_BRACE systemItemsWithBody CLOSE_BRACE
	;

systemItemsWithBody:
	systemItemsBeforeFirstBody bodyDeclaration systemItems
	;

systemItemsBeforeFirstBody:
	%empty
	| systemItemsBeforeFirstBody nonBodySystemItem
	;

systemItems:
	%empty
	| systemItems systemItem
	;

systemItem:
	nonBodySystemItem
	| bodyDeclaration
	;

nonBodySystemItem:
	unitsDeclaration
	| gravityDeclaration
	| surfaceDeclaration
	| referenceFrameDeclaration
	| distanceDeclaration
	;

unitsDeclaration:
	UNITS OPEN_BRACE unitDeclarationList CLOSE_BRACE
	;

unitDeclarationList:
	unitDeclaration
	| unitDeclarationList unitDeclaration
	;

unitDeclaration:
	MASS KG SEMICOLON
	| FORCE NEWTON SEMICOLON
	| DISTANCE METER SEMICOLON
	;

gravityDeclaration:
	GRAVITY NUMBER SEMICOLON
	;

surfaceDeclaration:
	SURFACE OPEN_BRACE TYPE HORIZONTAL SEMICOLON optionalFriction CLOSE_BRACE
	| SURFACE OPEN_BRACE TYPE INCLINE SEMICOLON ANGLE angleValue SEMICOLON optionalFriction CLOSE_BRACE
	;

optionalFriction:
	%empty
	| frictionDeclaration
	;

frictionDeclaration:
	FRICTION OPEN_BRACE STATIC NUMBER SEMICOLON KINETIC NUMBER SEMICOLON CLOSE_BRACE
	;

bodyDeclaration:
	BODY ID SEMICOLON
	| BODY ID OPEN_BRACE bodyItems CLOSE_BRACE
	;

bodyItems:
	%empty
	| bodyItems bodyItem
	;

bodyItem:
	bodyTypeDeclaration
	| massDeclaration
	| forceDeclaration
	| implicitForcesDeclaration
	;

bodyTypeDeclaration:
	TYPE BLOCK SEMICOLON
	| TYPE SPHERE SEMICOLON
	;

massDeclaration:
	MASS NUMBER optionalMassUnit SEMICOLON
	;

optionalMassUnit:
	%empty
	| KG
	;

forceDeclaration:
	FORCE ID OPEN_BRACE magnitudeDeclaration directionDeclaration CLOSE_BRACE
	;

magnitudeDeclaration:
	MAGNITUDE NUMBER optionalForceUnit SEMICOLON
	;

optionalForceUnit:
	%empty
	| NEWTON
	;

directionDeclaration:
	DIRECTION directionSpec SEMICOLON
	;

directionSpec:
	ANGLE angleValue
	| PARALLEL TO SURFACE
	;

implicitForcesDeclaration:
	IMPLICIT implicitForceList SEMICOLON
	;

implicitForceList:
	implicitForce
	| implicitForceList COMMA implicitForce
	;

implicitForce:
	WEIGHT
	| NORMAL
	| FRICTION
	;

referenceFrameDeclaration:
	REFERENCE FRAME ALIGNED WITH SURFACE ON ID SEMICOLON
	| REFERENCE FRAME ABSOLUTE ON ID SEMICOLON
	;

distanceDeclaration:
	DISTANCE ID ID OPEN_BRACE polarDistanceSpec CLOSE_BRACE
	| DISTANCE ID ID OPEN_BRACE cartesianDistanceSpec CLOSE_BRACE
	;

polarDistanceSpec:
	MAGNITUDE NUMBER optionalDistanceUnit SEMICOLON ANGLE angleValue SEMICOLON
	;

cartesianDistanceSpec:
	X_AXIS NUMBER optionalDistanceUnit SEMICOLON Y_AXIS NUMBER optionalDistanceUnit SEMICOLON
	;

optionalDistanceUnit:
	%empty
	| METER
	;

angleValue:
	NUMBER DEGREE
	;

%%
