/*
** 2000-05-29
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Driver template for the LEMON parser generator.
**
** The "lemon" program processes an LALR(1) input grammar file, then uses
** this template to construct a parser.  The "lemon" program inserts text
** at each "%%" line.  Also, any "P-a-r-s-e" identifer prefix (without the
** interstitial "-" characters) contained in this template is changed into
** the value of the %name directive from the grammar.  Otherwise, the content
** of this template is copied straight through into the generate parser
** source file.
**
** The following is the concatenation of all %include directives from the
** input grammar file:
*/
#include <stdio.h>
/************ Begin %include sections from the grammar ************************/
#line 48 "parse.y"

#include "sqliteInt.h"

/*
** Disable all error recovery processing in the parser push-down
** automaton.
*/
#define YYNOERRORRECOVERY 1

/*
** Make yytestcase() the same as testcase()
*/
#define yytestcase(X) testcase(X)

/*
** Indicate that sqlite3ParserFree() will never be called with a null
** pointer.
*/
#define YYPARSEFREENEVERNULL 1

/*
** Alternative datatype for the argument to the malloc() routine passed
** into sqlite3ParserAlloc().  The default is size_t.
*/
#define YYMALLOCARGTYPE  u64

/*
** An instance of this structure holds information about the
** LIMIT clause of a SELECT statement.
*/
struct LimitVal {
  Expr *pLimit;    /* The LIMIT expression.  NULL if there is no limit */
  Expr *pOffset;   /* The OFFSET expression.  NULL if there is none */
};

/*
** An instance of the following structure describes the event of a
** TRIGGER.  "a" is the event type, one of TK_UPDATE, TK_INSERT,
** TK_DELETE, or TK_INSTEAD.  If the event is of the form
**
**      UPDATE ON (a,b,c)
**
** Then the "b" IdList records the list "a,b,c".
*/
struct TrigEvent { int a; IdList * b; };

/*
** Disable lookaside memory allocation for objects that might be
** shared across database connections.
*/
static void disableLookaside(Parse *pParse){
  pParse->disableLookaside++;
  pParse->db->lookaside.bDisable++;
}

#line 396 "parse.y"

  /*
  ** For a compound SELECT statement, make sure p->pPrior->pNext==p for
  ** all elements in the list.  And make sure list length does not exceed
  ** SQLITE_LIMIT_COMPOUND_SELECT.
  */
  static void parserDoubleLinkSelect(Parse *pParse, Select *p){
    if( p->pPrior ){
      Select *pNext = 0, *pLoop;
      int mxSelect, cnt = 0;
      for(pLoop=p; pLoop; pNext=pLoop, pLoop=pLoop->pPrior, cnt++){
        pLoop->pNext = pNext;
        pLoop->selFlags |= SF_Compound;
      }
      if( (p->selFlags & SF_MultiValue)==0 && 
        (mxSelect = pParse->db->aLimit[SQLITE_LIMIT_COMPOUND_SELECT])>0 &&
        cnt>mxSelect
      ){
        sqlite3ErrorMsg(pParse, "Too many UNION or EXCEPT or INTERSECT operations");
      }
    }
  }
#line 833 "parse.y"

  /* This is a utility routine used to set the ExprSpan.zStart and
  ** ExprSpan.zEnd values of pOut so that the span covers the complete
  ** range of text beginning with pStart and going to the end of pEnd.
  */
  static void spanSet(ExprSpan *pOut, Token *pStart, Token *pEnd){
    pOut->zStart = pStart->z;
    pOut->zEnd = &pEnd->z[pEnd->n];
  }

  /* Construct a new Expr object from a single identifier.  Use the
  ** new Expr to populate pOut.  Set the span of pOut to be the identifier
  ** that created the expression.
  */
  static void spanExpr(ExprSpan *pOut, Parse *pParse, int op, Token t){
    Expr *p = sqlite3DbMallocRawNN(pParse->db, sizeof(Expr)+t.n+1);
    if( p ){
      memset(p, 0, sizeof(Expr));
      p->op = (u8)op;
      p->flags = EP_Leaf;
      p->iAgg = -1;
      p->u.zToken = (char*)&p[1];
      memcpy(p->u.zToken, t.z, t.n);
      p->u.zToken[t.n] = 0;
      if( sqlite3Isquote(p->u.zToken[0]) ){
        if( p->u.zToken[0]=='"' ) p->flags |= EP_DblQuoted;
        sqlite3Dequote(p->u.zToken);
      }
#if SQLITE_MAX_EXPR_DEPTH>0
      p->nHeight = 1;
#endif  
    }
    pOut->pExpr = p;
    pOut->zStart = t.z;
    pOut->zEnd = &t.z[t.n];
  }
#line 942 "parse.y"

  /* This routine constructs a binary expression node out of two ExprSpan
  ** objects and uses the result to populate a new ExprSpan object.
  */
  static void spanBinaryExpr(
    Parse *pParse,      /* The parsing context.  Errors accumulate here */
    int op,             /* The binary operation */
    ExprSpan *pLeft,    /* The left operand, and output */
    ExprSpan *pRight    /* The right operand */
  ){
    pLeft->pExpr = sqlite3PExpr(pParse, op, pLeft->pExpr, pRight->pExpr);
    pLeft->zEnd = pRight->zEnd;
  }

  /* If doNot is true, then add a TK_NOT Expr-node wrapper around the
  ** outside of *ppExpr.
  */
  static void exprNot(Parse *pParse, int doNot, ExprSpan *pSpan){
    if( doNot ){
      pSpan->pExpr = sqlite3PExpr(pParse, TK_NOT, pSpan->pExpr, 0);
    }
  }
#line 1016 "parse.y"

  /* Construct an expression node for a unary postfix operator
  */
  static void spanUnaryPostfix(
    Parse *pParse,         /* Parsing context to record errors */
    int op,                /* The operator */
    ExprSpan *pOperand,    /* The operand, and output */
    Token *pPostOp         /* The operand token for setting the span */
  ){
    pOperand->pExpr = sqlite3PExpr(pParse, op, pOperand->pExpr, 0);
    pOperand->zEnd = &pPostOp->z[pPostOp->n];
  }                           
#line 1033 "parse.y"

  /* A routine to convert a binary TK_IS or TK_ISNOT expression into a
  ** unary TK_ISNULL or TK_NOTNULL expression. */
  static void binaryToUnaryIfNull(Parse *pParse, Expr *pY, Expr *pA, int op){
    sqlite3 *db = pParse->db;
    if( pA && pY && pY->op==TK_NULL ){
      pA->op = (u8)op;
      sqlite3ExprDelete(db, pA->pRight);
      pA->pRight = 0;
    }
  }
#line 1061 "parse.y"

  /* Construct an expression node for a unary prefix operator
  */
  static void spanUnaryPrefix(
    ExprSpan *pOut,        /* Write the new expression node here */
    Parse *pParse,         /* Parsing context to record errors */
    int op,                /* The operator */
    ExprSpan *pOperand,    /* The operand */
    Token *pPreOp         /* The operand token for setting the span */
  ){
    pOut->zStart = pPreOp->z;
    pOut->pExpr = sqlite3PExpr(pParse, op, pOperand->pExpr, 0);
    pOut->zEnd = pOperand->zEnd;
  }
#line 1266 "parse.y"

  /* Add a single new term to an ExprList that is used to store a
  ** list of identifiers.  Report an error if the ID list contains
  ** a COLLATE clause or an ASC or DESC keyword, except ignore the
  ** error while parsing a legacy schema.
  */
  static ExprList *parserAddExprIdListTerm(
    Parse *pParse,
    ExprList *pPrior,
    Token *pIdToken,
    int hasCollate,
    int sortOrder
  ){
    ExprList *p = sqlite3ExprListAppend(pParse, pPrior, 0);
    if( (hasCollate || sortOrder!=SQLITE_SO_UNDEFINED)
        && pParse->db->init.busy==0
    ){
      sqlite3ErrorMsg(pParse, "syntax error after column name \"%.*s\"",
                         pIdToken->n, pIdToken->z);
    }
    sqlite3ExprListSetName(pParse, p, pIdToken, 1);
    return p;
  }
#line 231 "parse.c"
/**************** End of %include directives **********************************/
/* These constants specify the various numeric values for terminal symbols
** in a format understandable to "makeheaders".  This section is blank unless
** "lemon" is run with the "-m" command-line option.
***************** Begin makeheaders token definitions *************************/
/**************** End makeheaders token definitions ***************************/

/* The next sections is a series of control #defines.
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used to store the integer codes
**                       that represent terminal and non-terminal symbols.
**                       "unsigned char" is used if there are fewer than
**                       256 symbols.  Larger types otherwise.
**    YYNOCODE           is a number of type YYCODETYPE that is not used for
**                       any terminal or nonterminal symbol.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       (also known as: "terminal symbols") have fall-back
**                       values which should be used if the original symbol
**                       would not parse.  This permits keywords to sometimes
**                       be used as identifiers, for example.
**    YYACTIONTYPE       is the data type used for "action codes" - numbers
**                       that indicate what to do in response to the next
**                       token.
**    sqlite3ParserTOKENTYPE     is the data type used for minor type for terminal
**                       symbols.  Background: A "minor type" is a semantic
**                       value associated with a terminal or non-terminal
**                       symbols.  For example, for an "ID" terminal symbol,
**                       the minor type might be the name of the identifier.
**                       Each non-terminal can have a different minor type.
**                       Terminal symbols all have the same minor type, though.
**                       This macros defines the minor type for terminal 
**                       symbols.
**    YYMINORTYPE        is the data type used for all minor types.
**                       This is typically a union of many types, one of
**                       which is sqlite3ParserTOKENTYPE.  The entry in the union
**                       for terminal symbols is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    sqlite3ParserARG_SDECL     A static variable declaration for the %extra_argument
**    sqlite3ParserARG_PDECL     A parameter declaration for the %extra_argument
**    sqlite3ParserARG_STORE     Code to store %extra_argument into yypParser
**    sqlite3ParserARG_FETCH     Code to extract %extra_argument from yypParser
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YY_MAX_SHIFT       Maximum value for shift actions
**    YY_MIN_SHIFTREDUCE Minimum value for shift-reduce actions
**    YY_MAX_SHIFTREDUCE Maximum value for shift-reduce actions
**    YY_MIN_REDUCE      Maximum value for reduce actions
**    YY_ERROR_ACTION    The yy_action[] code for syntax error
**    YY_ACCEPT_ACTION   The yy_action[] code for accept
**    YY_NO_ACTION       The yy_action[] code for no-op
*/
#ifndef INTERFACE
# define INTERFACE 1
#endif
/************* Begin control #defines *****************************************/
#define YYCODETYPE unsigned char
#define YYNOCODE 237
#define YYACTIONTYPE unsigned short int
#define YYWILDCARD 92
#define sqlite3ParserTOKENTYPE Token
typedef union {
  int yyinit;
  sqlite3ParserTOKENTYPE yy0;
  SrcList* yy3;
  TriggerStep* yy19;
  struct LimitVal yy196;
  With* yy211;
  int yy228;
  struct {int value; int mask;} yy231;
  ExprList* yy258;
  ExprSpan yy326;
  Select* yy387;
  IdList* yy400;
  Expr* yy402;
  struct TrigEvent yy466;
} YYMINORTYPE;
#ifndef YYSTACKDEPTH
#define YYSTACKDEPTH 100
#endif
#define sqlite3ParserARG_SDECL Parse *pParse;
#define sqlite3ParserARG_PDECL ,Parse *pParse
#define sqlite3ParserARG_FETCH Parse *pParse = yypParser->pParse
#define sqlite3ParserARG_STORE yypParser->pParse = pParse
#define YYFALLBACK 1
#define YYNSTATE             420
#define YYNRULE              304
#define YY_MAX_SHIFT         419
#define YY_MIN_SHIFTREDUCE   616
#define YY_MAX_SHIFTREDUCE   919
#define YY_MIN_REDUCE        920
#define YY_MAX_REDUCE        1223
#define YY_ERROR_ACTION      1224
#define YY_ACCEPT_ACTION     1225
#define YY_NO_ACTION         1226
/************* End control #defines *******************************************/

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif


/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N <= YY_MAX_SHIFT             Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   N between YY_MIN_SHIFTREDUCE       Shift to an arbitrary state then
**     and YY_MAX_SHIFTREDUCE           reduce by rule N-YY_MIN_SHIFTREDUCE.
**
**   N between YY_MIN_REDUCE            Reduce by rule N-YY_MIN_REDUCE
**     and YY_MAX_REDUCE
**
**   N == YY_ERROR_ACTION               A syntax error has occurred.
**
**   N == YY_ACCEPT_ACTION              The parser accepts its input.
**
**   N == YY_NO_ACTION                  No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as either:
**
**    (A)   N = yy_action[ yy_shift_ofst[S] + X ]
**    (B)   N = yy_default[S]
**
** The (A) formula is preferred.  The B formula is used instead if:
**    (1)  The yy_shift_ofst[S]+X value is out of range, or
**    (2)  yy_lookahead[yy_shift_ofst[S]+X] is not equal to X, or
**    (3)  yy_shift_ofst[S] equal YY_SHIFT_USE_DFLT.
** (Implementation note: YY_SHIFT_USE_DFLT is chosen so that
** YY_SHIFT_USE_DFLT+X will be out of range for all possible lookaheads X.
** Hence only tests (1) and (2) need to be evaluated.)
**
** The formulas above are for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array and YY_REDUCE_USE_DFLT is used in place of
** YY_SHIFT_USE_DFLT.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
**
*********** Begin parsing tables **********************************************/
#define YY_ACTTAB_COUNT (1427)
static const YYACTIONTYPE yy_action[] = {
 /*     0 */    91,   92,  293,   82,  784,  784,  796,  799,  788,  788,
 /*    10 */    89,   89,   90,   90,   90,   90,  315,   88,   88,   88,
 /*    20 */    88,   87,   87,   86,   86,   86,   85,  315,   90,   90,
 /*    30 */    90,   90,   83,   88,   88,   88,   88,   87,   87,   86,
 /*    40 */    86,   86,   85,  315,  198,  198,  270,  904,  904,  343,
 /*    50 */    91,   92,  293,   82,  784,  784,  796,  799,  788,  788,
 /*    60 */    89,   89,   90,   90,   90,   90,  124,   88,   88,   88,
 /*    70 */    88,   87,   87,   86,   86,   86,   85,  315,   87,   87,
 /*    80 */    86,   86,   86,   85,  315,  358,  643,  191,  904,  904,
 /*    90 */    88,   88,   88,   88,   87,   87,   86,   86,   86,   85,
 /*   100 */   315,  418,  418,  734,  705,  617,  318,  227,  375,  123,
 /*   110 */   697,  697,  735,  211,  692,   84,   81,  172,   91,   92,
 /*   120 */   293,   82,  784,  784,  796,  799,  788,  788,   89,   89,
 /*   130 */    90,   90,   90,   90,  408,   88,   88,   88,   88,   87,
 /*   140 */    87,   86,   86,   86,   85,  315,  646,   22,   91,   92,
 /*   150 */   293,   82,  784,  784,  796,  799,  788,  788,   89,   89,
 /*   160 */    90,   90,   90,   90,   67,   88,   88,   88,   88,   87,
 /*   170 */    87,   86,   86,   86,   85,  315,   86,   86,   86,   85,
 /*   180 */   315,  295,   85,  315,  886,  886,  252,  733, 1225,  419,
 /*   190 */     3,  249,   93,   84,   81,  172,  760,   91,   92,  293,
 /*   200 */    82,  784,  784,  796,  799,  788,  788,   89,   89,   90,
 /*   210 */    90,   90,   90,  896,   88,   88,   88,   88,   87,   87,
 /*   220 */    86,   86,   86,   85,  315,  645,  887,  190,  679,  658,
 /*   230 */   366,  363,  362,   84,   81,  172,  774,  679,  767,  381,
 /*   240 */   651,  361,  762,   91,   92,  293,   82,  784,  784,  796,
 /*   250 */   799,  788,  788,   89,   89,   90,   90,   90,   90,  334,
 /*   260 */    88,   88,   88,   88,   87,   87,   86,   86,   86,   85,
 /*   270 */   315,  414,  111,  766,  766,  768,  313,  312,  247,  226,
 /*   280 */   376,  785,  785,  797,  800,  732,  644,  166,  168,   48,
 /*   290 */    48,  718,   91,   92,  293,   82,  784,  784,  796,  799,
 /*   300 */   788,  788,   89,   89,   90,   90,   90,   90,  345,   88,
 /*   310 */    88,   88,   88,   87,   87,   86,   86,   86,   85,  315,
 /*   320 */   414,  238,  246,  190,  392,  377,  366,  363,  362,  662,
 /*   330 */   321,   84,   81,  172,  819,  775,  661,  361,   48,   48,
 /*   340 */   718,   91,   92,  293,   82,  784,  784,  796,  799,  788,
 /*   350 */   788,   89,   89,   90,   90,   90,   90,  414,   88,   88,
 /*   360 */    88,   88,   87,   87,   86,   86,   86,   85,  315,  789,
 /*   370 */   412,  412,  412,  392,  382,   10,   10,  319,  913,  229,
 /*   380 */   913,  314,  314,  314,  761,  660,  842,  842,  335,  301,
 /*   390 */    91,   92,  293,   82,  784,  784,  796,  799,  788,  788,
 /*   400 */    89,   89,   90,   90,   90,   90,  765,   88,   88,   88,
 /*   410 */    88,   87,   87,   86,   86,   86,   85,  315,   91,   92,
 /*   420 */   293,   82,  784,  784,  796,  799,  788,  788,   89,   89,
 /*   430 */    90,   90,   90,   90,  221,   88,   88,   88,   88,   87,
 /*   440 */    87,   86,   86,   86,   85,  315,   91,   92,  293,   82,
 /*   450 */   784,  784,  796,  799,  788,  788,   89,   89,   90,   90,
 /*   460 */    90,   90,  712,   88,   88,   88,   88,   87,   87,   86,
 /*   470 */    86,   86,   85,  315,   91,   92,  293,   82,  784,  784,
 /*   480 */   796,  799,  788,  788,   89,   89,   90,   90,   90,   90,
 /*   490 */   147,   88,   88,   88,   88,   87,   87,   86,   86,   86,
 /*   500 */    85,  315,  371,   91,   80,  293,   82,  784,  784,  796,
 /*   510 */   799,  788,  788,   89,   89,   90,   90,   90,   90,   70,
 /*   520 */    88,   88,   88,   88,   87,   87,   86,   86,   86,   85,
 /*   530 */   315,  167,   92,  293,   82,  784,  784,  796,  799,  788,
 /*   540 */   788,   89,   89,   90,   90,   90,   90,   73,   88,   88,
 /*   550 */    88,   88,   87,   87,   86,   86,   86,   85,  315,  293,
 /*   560 */    82,  784,  784,  796,  799,  788,  788,   89,   89,   90,
 /*   570 */    90,   90,   90,   78,   88,   88,   88,   88,   87,   87,
 /*   580 */    86,   86,   86,   85,  315,    5,  702,  198,  684,  684,
 /*   590 */   904,  701,   75,   76,  124,   90,   90,   90,   90,   77,
 /*   600 */    88,   88,   88,   88,   87,   87,   86,   86,   86,   85,
 /*   610 */   315,  138,  410,    2, 1117,  892,  886,  886,  316,  316,
 /*   620 */   414, 1174, 1174,  653,  386,  414,   78,  756,  169,  413,
 /*   630 */   287,  904,  124,  285,  284,  283,  208,  281,   48,   48,
 /*   640 */   630,  313,  312,   48,   48,   75,   76,  399,  242,  340,
 /*   650 */   241,  258,   77,  174,  124,  682,  682,  774,  887,  767,
 /*   660 */   415,  414,  257,  762,  694,  410,    2,  886,  886,  414,
 /*   670 */   373,  316,  316,  308,  307,  379,  178,  325,  387,   30,
 /*   680 */    30,  760,  413,  380,  124,  176,  414,   48,   48,  349,
 /*   690 */   886,  886,  325,  324,  766,  766,  768,  769,   18,  304,
 /*   700 */   399,    9,    9,  111,   48,   48,  294,  124,  908,  887,
 /*   710 */   774,  912,  767,  415,  374,   78,  762,  397,  910,  414,
 /*   720 */   911,  268,  392,  391,  343,  139,  855,  164,  163,  162,
 /*   730 */   886,  886,  887,  322,   75,   76,  706,   47,   47,  392,
 /*   740 */   372,   77,  867,  913,  298,  913,  146,  766,  766,  768,
 /*   750 */   769,   18,  280,  325,  410,    2,  289,  718,   78,  111,
 /*   760 */   316,  316,  205,  157,  259,  369,  254,  368,  192,  203,
 /*   770 */   203,  413,  887,  214,  414,  252,  354,   75,   76,  351,
 /*   780 */   349,  379,  341,  393,   77,  159,  158,  756,  215,  399,
 /*   790 */   269,  866,   48,   48,  328,   23,  212,  410,    2,  774,
 /*   800 */   339,  767,  415,  316,  316,  762,   95,  864,  242,  330,
 /*   810 */   230,  647,  647,  225,  413,  414,  111,  203,  203,  226,
 /*   820 */   376,  146,  855,   54,  718,  107,  858,  392,  394,  379,
 /*   830 */   641,  288,  399,   10,   10,  414,  766,  766,  768,  769,
 /*   840 */    18,  859,  774,  414,  767,  415,   68,  303,  762,  836,
 /*   850 */   774,  383,  767,   10,   10,  111,  762,  860,  326,  685,
 /*   860 */   323,   10,   10,  213,  240,   75,   76,  217,  886,  886,
 /*   870 */   686,  641,   77,  829,  831,  389,  707,  833,  341,  766,
 /*   880 */   766,  768,  769,   18,  718,  410,    2,  766,  766,  768,
 /*   890 */   665,  316,  316,  146,  756,  349,  350,  384,  143,  124,
 /*   900 */   886,  886,  413,  292,  367,  194,  886,  886,  719,  414,
 /*   910 */   887,  235,  666,   24,  858,  242,  340,  241,  886,  886,
 /*   920 */   399,  760,  218,  861,  218,  637,  296,   34,   34,  859,
 /*   930 */   774,  270,  767,  415,  829,  305,  762,  839,  886,  886,
 /*   940 */   414,  838,  887,  414,    1,  860,  414,  398,  887,  886,
 /*   950 */   886,  170,  271,  271,  232,  234,  333,  414,   35,   35,
 /*   960 */   887,   36,   36,  338,   37,   37,  297,  766,  766,  768,
 /*   970 */   769,   18,  760,  827,  721,   38,   38,  719,  760,  216,
 /*   980 */   887,  414,  124,  414,  299,  302,  311,  199,  168,  414,
 /*   990 */   327,  887,  414,  718,  414,  180,  414,  753,  414,   26,
 /*  1000 */    26,   27,   27,  414,  233,  414,  720,   29,   29,  414,
 /*  1010 */    39,   39,   40,   40,   41,   41,   11,   11,  414,  404,
 /*  1020 */   414,   42,   42,   97,   97,  347,  414,   43,   43,  414,
 /*  1030 */   111,  414,  331,  261,  414,  300,   44,   44,   31,   31,
 /*  1040 */   414,  309,  414,  181,   45,   45,  414,   46,   46,   32,
 /*  1050 */    32,  414,  113,  113,  414,  191,  414,  850,  114,  114,
 /*  1060 */   115,  115,  414,  889,   52,   52,  414,  161,  414,   33,
 /*  1070 */    33,  348,   98,   98,   49,   49,  414,  849,  414,  719,
 /*  1080 */    99,   99,  414,  352,  100,  100,   96,   96,  414,  718,
 /*  1090 */   414,  353,  414,  718,  112,  112,  110,  110,  414,  718,
 /*  1100 */   104,  104,  414,  853,  889,  414,  103,  103,  101,  101,
 /*  1110 */   102,  102,  414,  411,  296,  414,   51,   51,  690,  634,
 /*  1120 */    53,   53,  170,   50,   50,  401,  405,  409,  691,  204,
 /*  1130 */    25,   25,  687,   28,   28,  702,  111,   66,  237,  903,
 /*  1140 */   701, 1199,   64,   20,  674,  734,  654,  758,  719,  295,
 /*  1150 */   197,  111,  111,   74,  735,   72,  664,  663,  111,  111,
 /*  1160 */   111,  109,  671,  151,  342,  344,  243,  197,  197,   66,
 /*  1170 */   359,  826,  250,  200,   19,   66,  699,  728,  245,   69,
 /*  1180 */   197,  822,  629,  835,  200,  835,  770,  654,  656,  834,
 /*  1190 */   639,  834,  248,  106,  675,  659,  253,  263,  265,  672,
 /*  1200 */  1190,  726,  148,  759,  708,  272,  273,  155,    7,  642,
 /*  1210 */   636,  627,  826,  626,  628,  880,  748,  329,  239,  852,
 /*  1220 */   231,  346,  364,  658,  400,  278,  267,  770,  160,  156,
 /*  1230 */   883,  919,  125,  256,  136,  119,   64,  824,  823,  145,
 /*  1240 */   332,   55,  337,  837,  357,  144,  745,  182,  186,  236,
 /*  1250 */   149,  370,  187,  127,  355,  188,  678,  129,  130,  131,
 /*  1260 */   132,  140,  290,  755,  677,  676,  306,  656,  385,   63,
 /*  1270 */   854,  818,  291,  650,  649,    6,  669,   71,  255,  668,
 /*  1280 */   648,  310,  894,   94,  390,   65,  388,  716,  206,  210,
 /*  1290 */    21,  260,  416,  403,  717,  881,  633,  207,  262,  209,
 /*  1300 */   715,  407,  417,  624,  622,  623,  620,  619,  116,  286,
 /*  1310 */   117,  118,  317,  228,  105,  264,  219,  173,  714,  320,
 /*  1320 */   266,  108,  175,  177,  832,  830,  754,  126,  121,  179,
 /*  1330 */   698,  276,  274,  275,  277,  128,  688,  804,  840,  197,
 /*  1340 */   222,  133,  915,  336,  134,  848,  135,  223,   56,  137,
 /*  1350 */    57,   58,  224,   59,  122,  851,  183,  847,  184,    8,
 /*  1360 */    12,  185,  632,  150,  356,  189,  141,  244,  360,  257,
 /*  1370 */    60,   13,  667,  365,  251,   14,   61,  220,  773,  772,
 /*  1380 */   120,  802,  696,  165,  202,   15,    4,   62,  700,  722,
 /*  1390 */   193,  195,  378,  616,  142, 1179,  922,  395,  727,   69,
 /*  1400 */   817,   66,  803,  801,  396,  806,  857,  196,  856,  171,
 /*  1410 */    16,  873,   17,  152,  402,  201,  874,  153,  406,  805,
 /*  1420 */   154,  282,  771,  640,   79, 1191,  279,
};
static const YYCODETYPE yy_lookahead[] = {
 /*     0 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*    10 */    15,   16,   17,   18,   19,   20,   32,   22,   23,   24,
 /*    20 */    25,   26,   27,   28,   29,   30,   31,   32,   17,   18,
 /*    30 */    19,   20,   21,   22,   23,   24,   25,   26,   27,   28,
 /*    40 */    29,   30,   31,   32,   49,   49,  147,   52,   52,  147,
 /*    50 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*    60 */    15,   16,   17,   18,   19,   20,   88,   22,   23,   24,
 /*    70 */    25,   26,   27,   28,   29,   30,   31,   32,   26,   27,
 /*    80 */    28,   29,   30,   31,   32,    7,  165,    9,   93,   93,
 /*    90 */    22,   23,   24,   25,   26,   27,   28,   29,   30,   31,
 /*   100 */    32,  143,  144,   58,  203,    1,    2,  149,  111,  151,
 /*   110 */   113,  114,   67,  211,  156,  214,  215,  216,    5,    6,
 /*   120 */     7,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   130 */    17,   18,   19,   20,  235,   22,   23,   24,   25,   26,
 /*   140 */    27,   28,   29,   30,   31,   32,  165,  189,    5,    6,
 /*   150 */     7,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   160 */    17,   18,   19,   20,   51,   22,   23,   24,   25,   26,
 /*   170 */    27,   28,   29,   30,   31,   32,   28,   29,   30,   31,
 /*   180 */    32,  103,   31,   32,   52,   53,  108,  168,  140,  141,
 /*   190 */   142,   48,   79,  214,  215,  216,  147,    5,    6,    7,
 /*   200 */     8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
 /*   210 */    18,   19,   20,  178,   22,   23,   24,   25,   26,   27,
 /*   220 */    28,   29,   30,   31,   32,  165,   94,   95,  172,  173,
 /*   230 */    98,   99,  100,  214,  215,  216,   91,  181,   93,  156,
 /*   240 */    48,  109,   97,    5,    6,    7,    8,    9,   10,   11,
 /*   250 */    12,   13,   14,   15,   16,   17,   18,   19,   20,  210,
 /*   260 */    22,   23,   24,   25,   26,   27,   28,   29,   30,   31,
 /*   270 */    32,  147,  189,  128,  129,  130,   26,   27,   43,  115,
 /*   280 */   116,    9,   10,   11,   12,  168,   48,  204,  205,  165,
 /*   290 */   166,  147,    5,    6,    7,    8,    9,   10,   11,   12,
 /*   300 */    13,   14,   15,   16,   17,   18,   19,   20,  147,   22,
 /*   310 */    23,   24,   25,   26,   27,   28,   29,   30,   31,   32,
 /*   320 */   147,   86,   87,   95,  200,  201,   98,   99,  100,  174,
 /*   330 */   186,  214,  215,  216,   99,   48,  174,  109,  165,  166,
 /*   340 */   147,    5,    6,    7,    8,    9,   10,   11,   12,   13,
 /*   350 */    14,   15,   16,   17,   18,   19,   20,  147,   22,   23,
 /*   360 */    24,   25,   26,   27,   28,   29,   30,   31,   32,   97,
 /*   370 */   161,  162,  163,  200,  201,  165,  166,  233,  128,  186,
 /*   380 */   130,  161,  162,  163,   48,  174,  104,  105,  106,  179,
 /*   390 */     5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
 /*   400 */    15,   16,   17,   18,   19,   20,  147,   22,   23,   24,
 /*   410 */    25,   26,   27,   28,   29,   30,   31,   32,    5,    6,
 /*   420 */     7,    8,    9,   10,   11,   12,   13,   14,   15,   16,
 /*   430 */    17,   18,   19,   20,  203,   22,   23,   24,   25,   26,
 /*   440 */    27,   28,   29,   30,   31,   32,    5,    6,    7,    8,
 /*   450 */     9,   10,   11,   12,   13,   14,   15,   16,   17,   18,
 /*   460 */    19,   20,  206,   22,   23,   24,   25,   26,   27,   28,
 /*   470 */    29,   30,   31,   32,    5,    6,    7,    8,    9,   10,
 /*   480 */    11,   12,   13,   14,   15,   16,   17,   18,   19,   20,
 /*   490 */    49,   22,   23,   24,   25,   26,   27,   28,   29,   30,
 /*   500 */    31,   32,   28,    5,    6,    7,    8,    9,   10,   11,
 /*   510 */    12,   13,   14,   15,   16,   17,   18,   19,   20,  134,
 /*   520 */    22,   23,   24,   25,   26,   27,   28,   29,   30,   31,
 /*   530 */    32,  147,    6,    7,    8,    9,   10,   11,   12,   13,
 /*   540 */    14,   15,   16,   17,   18,   19,   20,  134,   22,   23,
 /*   550 */    24,   25,   26,   27,   28,   29,   30,   31,   32,    7,
 /*   560 */     8,    9,   10,   11,   12,   13,   14,   15,   16,   17,
 /*   570 */    18,   19,   20,    7,   22,   23,   24,   25,   26,   27,
 /*   580 */    28,   29,   30,   31,   32,   47,  112,   49,  183,  184,
 /*   590 */    52,  117,   26,   27,   88,   17,   18,   19,   20,   33,
 /*   600 */    22,   23,   24,   25,   26,   27,   28,   29,   30,   31,
 /*   610 */    32,   47,   46,   47,   48,  164,   52,   53,   52,   53,
 /*   620 */   147,  115,  116,  172,  147,  147,    7,   83,  147,   63,
 /*   630 */    34,   93,   88,   37,   38,   39,   40,   41,  165,  166,
 /*   640 */    44,   26,   27,  165,  166,   26,   27,   81,  104,  105,
 /*   650 */   106,   97,   33,   57,   88,  183,  184,   91,   94,   93,
 /*   660 */    94,  147,  108,   97,  188,   46,   47,   52,   53,  147,
 /*   670 */   156,   52,   53,  200,    7,  199,   80,  147,  200,  165,
 /*   680 */   166,  147,   63,  147,   88,   89,  147,  165,  166,  147,
 /*   690 */    52,   53,  162,  163,  128,  129,  130,  131,  132,   32,
 /*   700 */    81,  165,  166,  189,  165,  166,  110,   88,   93,   94,
 /*   710 */    91,   96,   93,   94,  200,    7,   97,  184,  103,  147,
 /*   720 */   105,  147,  200,  201,  147,   47,  156,  104,  105,  106,
 /*   730 */    52,   53,   94,  137,   26,   27,   28,  165,  166,  200,
 /*   740 */   201,   33,  147,  128,  210,  130,  147,  128,  129,  130,
 /*   750 */   131,  132,  153,  223,   46,   47,  157,  147,    7,  189,
 /*   760 */    52,   53,   95,   96,   97,   98,   99,  100,  101,  187,
 /*   770 */   188,   63,   94,  231,  147,  108,  221,   26,   27,  224,
 /*   780 */   147,  199,  212,  156,   33,   26,   27,   83,  211,   81,
 /*   790 */   218,  147,  165,  166,  212,  225,  186,   46,   47,   91,
 /*   800 */   230,   93,   94,   52,   53,   97,   47,  147,  104,  105,
 /*   810 */   106,   52,   53,  192,   63,  147,  189,  187,  188,  115,
 /*   820 */   116,  147,  156,  202,  147,   47,   39,  200,  201,  199,
 /*   830 */    52,  157,   81,  165,  166,  147,  128,  129,  130,  131,
 /*   840 */   132,   54,   91,  147,   93,   94,    7,  179,   97,   38,
 /*   850 */    91,    7,   93,  165,  166,  189,   97,   70,  147,   72,
 /*   860 */   147,  165,  166,  186,  231,   26,   27,  179,   52,   53,
 /*   870 */    83,   93,   33,  162,  163,  179,   28,  147,  212,  128,
 /*   880 */   129,  130,  131,  132,  147,   46,   47,  128,  129,  130,
 /*   890 */    62,   52,   53,  147,   83,  147,  230,   53,   82,   88,
 /*   900 */    52,   53,   63,  157,   76,   48,   52,   53,   51,  147,
 /*   910 */    94,   43,   84,   47,   39,  104,  105,  106,   52,   53,
 /*   920 */    81,  147,  176,  186,  178,  159,  160,  165,  166,   54,
 /*   930 */    91,  147,   93,   94,  223,  107,   97,   56,   52,   53,
 /*   940 */   147,   60,   94,  147,   47,   70,  147,   72,   94,   52,
 /*   950 */    53,   94,  147,  147,   86,   87,   75,  147,  165,  166,
 /*   960 */    94,  165,  166,  227,  165,  166,  147,  128,  129,  130,
 /*   970 */   131,  132,  147,  147,  120,  165,  166,  120,  147,  231,
 /*   980 */    94,  147,   88,  147,  210,  180,  180,  204,  205,  147,
 /*   990 */    96,   94,  147,  147,  147,  226,  147,  156,  147,  165,
 /*  1000 */   166,  165,  166,  147,  136,  147,  120,  165,  166,  147,
 /*  1010 */   165,  166,  165,  166,  165,  166,  165,  166,  147,  235,
 /*  1020 */   147,  165,  166,  165,  166,    7,  147,  165,  166,  147,
 /*  1030 */   189,  147,  186,  203,  147,  210,  165,  166,  165,  166,
 /*  1040 */   147,  210,  147,  147,  165,  166,  147,  165,  166,  165,
 /*  1050 */   166,  147,  165,  166,  147,    9,  147,  147,  165,  166,
 /*  1060 */   165,  166,  147,   52,  165,  166,  147,   51,  147,  165,
 /*  1070 */   166,   53,  165,  166,  165,  166,  147,  147,  147,   51,
 /*  1080 */   165,  166,  147,  147,  165,  166,  165,  166,  147,  147,
 /*  1090 */   147,  147,  147,  147,  165,  166,  165,  166,  147,  147,
 /*  1100 */   165,  166,  147,  156,   93,  147,  165,  166,  165,  166,
 /*  1110 */   165,  166,  147,  159,  160,  147,  165,  166,  156,  156,
 /*  1120 */   165,  166,   94,  165,  166,  156,  156,  156,  186,   47,
 /*  1130 */   165,  166,  186,  165,  166,  112,  189,   51,  186,   51,
 /*  1140 */   117,   48,  126,   16,   51,   58,   52,   48,  120,  103,
 /*  1150 */    51,  189,  189,  133,   67,  135,   96,   97,  189,  189,
 /*  1160 */   189,   47,   36,   49,   48,   48,   48,   51,   51,   51,
 /*  1170 */    48,   52,   48,   51,   47,   51,   48,   48,  147,   51,
 /*  1180 */    51,   48,  147,  128,   51,  130,   52,   93,  102,  128,
 /*  1190 */    48,  130,  147,   51,  147,  147,  147,  203,  203,   73,
 /*  1200 */   118,  147,  190,  147,  147,  147,  147,  119,  191,  147,
 /*  1210 */   147,  147,   93,  147,  147,  147,  194,  207,  232,  194,
 /*  1220 */   207,  232,  169,  173,  220,  193,  207,   93,  177,  191,
 /*  1230 */   150,   64,  234,  168,   47,    5,  126,  168,  168,  213,
 /*  1240 */    45,  133,   71,  229,   45,   47,  194,  152,  152,  228,
 /*  1250 */   213,  103,  152,  182,  170,  152,  167,  185,  185,  185,
 /*  1260 */   185,  182,  170,  182,  167,  167,   74,  102,  121,  103,
 /*  1270 */   194,  194,  170,  167,  169,   47,  175,  133,  167,  175,
 /*  1280 */   167,   32,  167,  125,  122,  124,  123,  209,   50,   35,
 /*  1290 */    51,  208,  154,  170,  209,   40,  155,  148,  208,  148,
 /*  1300 */   209,  170,  146,  146,   36,  146,  146,    4,  158,  145,
 /*  1310 */   158,  158,    3,  138,  171,  208,  171,   42,  209,   90,
 /*  1320 */   208,   43,  103,  118,   48,   48,  116,  127,  107,  103,
 /*  1330 */   198,  195,  197,  196,  194,  119,   46,  217,   78,   51,
 /*  1340 */   219,   78,   85,   69,  103,    1,  119,  222,   16,  127,
 /*  1350 */    16,   16,  222,   16,  107,   53,   61,    1,  118,   34,
 /*  1360 */    47,  103,   46,   49,    7,  101,   47,  136,   77,  108,
 /*  1370 */    47,   47,   55,   77,   48,   47,   47,   77,   48,   48,
 /*  1380 */    65,   48,  112,  118,  118,   47,   47,   51,   48,  120,
 /*  1390 */    48,   48,   51,    1,   47,    0,  236,   93,   53,   51,
 /*  1400 */    48,   51,   48,   48,   51,   38,   48,   61,   48,   47,
 /*  1410 */    61,   48,   61,   47,   49,   51,   48,   47,   49,   48,
 /*  1420 */    47,   42,   48,   48,   47,  118,   48,
};
#define YY_SHIFT_USE_DFLT (1427)
#define YY_SHIFT_COUNT    (419)
#define YY_SHIFT_MIN      (-22)
#define YY_SHIFT_MAX      (1395)
static const short yy_shift_ofst[] = {
 /*     0 */   104,  566,  619,  596,  751,  751,  751,  751,  544,   -5,
 /*    10 */    45,   45,  751,  751,  751,  751,  751,  751,  751,  615,
 /*    20 */   615,  132,  704,  811,  506,  113,  143,  192,  238,  287,
 /*    30 */   336,  385,  413,  441,  469,  469,  469,  469,  469,  469,
 /*    40 */   469,  469,  469,  469,  469,  469,  469,  469,  469,  498,
 /*    50 */   469,  526,  552,  552,  708,  751,  751,  751,  751,  751,
 /*    60 */   751,  751,  751,  751,  751,  751,  751,  751,  751,  751,
 /*    70 */   751,  751,  751,  751,  751,  751,  751,  751,  751,  751,
 /*    80 */   751,  751,  839,  751,  751,  751,  751,  751,  751,  751,
 /*    90 */   751,  751,  751,  751,  751,  751,   11,  578,  578,  578,
 /*   100 */   578,  578,   68,   52,  148,   78,  250,  250,  638,  638,
 /*   110 */   151,  164,  -16, 1427, 1427, 1427,  667,  667,  667,  787,
 /*   120 */   787,  564,  564,  235,  816,  638,  638,  638,  638,  638,
 /*   130 */   638,  638,  638,  638,  638,  638,  638,  638,  638,  638,
 /*   140 */   638,  638,  638,  638,  638,  894, 1011, 1011,  164,  -22,
 /*   150 */   -22,  -22,  -22,  -22,  -22, 1427, 1427,  759,  145,  145,
 /*   160 */   228,  678,  828,  828,  828,  848,  857,  538,  866,  854,
 /*   170 */   886,  875,  897,  638,  638,  638,  638,  638,  638,  638,
 /*   180 */   282,  881,  638,  638,  638,  638,  638,  638,  638,  638,
 /*   190 */   638,  638,  638,   -4,   -4,   -4,  638,  638,  638, 1028,
 /*   200 */   638,  638,  638,   -3,  474,  638,  638,  638,  638,  638,
 /*   210 */   638, 1016,  844,  844, 1018, 1016, 1018, 1086, 1093, 1046,
 /*   220 */  1087,  844, 1020, 1087, 1087, 1088, 1023, 1114, 1167, 1187,
 /*   230 */  1230, 1110, 1195, 1195, 1195, 1195, 1108, 1171, 1199, 1110,
 /*   240 */  1187, 1230, 1230, 1110, 1199, 1198, 1199, 1199, 1198, 1148,
 /*   250 */  1148, 1148, 1192, 1198, 1148, 1165, 1148, 1192, 1148, 1148,
 /*   260 */  1147, 1166, 1147, 1166, 1147, 1166, 1147, 1166, 1228, 1144,
 /*   270 */  1198, 1249, 1249, 1198, 1158, 1162, 1161, 1163, 1110, 1238,
 /*   280 */  1239, 1255, 1255, 1254, 1254, 1254, 1254, 1268, 1427, 1427,
 /*   290 */  1427, 1427, 1427,  272,  868,  623,  778, 1127, 1099, 1116,
 /*   300 */  1117, 1118, 1122, 1124, 1094, 1060, 1126,  554, 1128, 1129,
 /*   310 */  1119, 1133, 1055, 1061, 1142, 1134, 1082, 1303, 1309, 1175,
 /*   320 */  1275, 1229, 1278, 1219, 1276, 1277, 1205, 1210, 1200, 1221,
 /*   330 */  1216, 1226, 1290, 1260, 1288, 1263, 1257, 1274, 1241, 1344,
 /*   340 */  1227, 1222, 1332, 1334, 1335, 1337, 1247, 1302, 1295, 1240,
 /*   350 */  1356, 1325, 1313, 1258, 1231, 1314, 1316, 1357, 1261, 1264,
 /*   360 */  1319, 1291, 1323, 1324, 1326, 1328, 1296, 1317, 1329, 1300,
 /*   370 */  1315, 1330, 1331, 1333, 1336, 1270, 1338, 1340, 1339, 1341,
 /*   380 */  1265, 1342, 1343, 1345, 1346, 1347, 1269, 1348, 1349, 1350,
 /*   390 */  1351, 1352, 1348, 1354, 1355, 1358, 1304, 1353, 1360, 1362,
 /*   400 */  1367, 1363, 1366, 1365, 1364, 1368, 1370, 1369, 1364, 1371,
 /*   410 */  1373, 1374, 1375, 1377, 1266, 1307, 1378, 1379, 1392, 1395,
};
#define YY_REDUCE_USE_DFLT (-102)
#define YY_REDUCE_COUNT (292)
#define YY_REDUCE_MIN   (-101)
#define YY_REDUCE_MAX   (1164)
static const short yy_reduce_ofst[] = {
 /*     0 */    48,  627,  514,  -42,  124,  173,  522,  539,  570,  -99,
 /*    10 */    19,  117,  210,  668,  688,  473,  478,  696,  572,  530,
 /*    20 */   711,  746,  582,  666,   83,  -21,  -21,  -21,  -21,  -21,
 /*    30 */   -21,  -21,  -21,  -21,  -21,  -21,  -21,  -21,  -21,  -21,
 /*    40 */   -21,  -21,  -21,  -21,  -21,  -21,  -21,  -21,  -21,  -21,
 /*    50 */   -21,  -21,  -21,  -21,  536,  762,  793,  796,  799,  810,
 /*    60 */   834,  836,  842,  845,  847,  849,  851,  856,  858,  862,
 /*    70 */   871,  873,  879,  882,  884,  887,  893,  895,  899,  904,
 /*    80 */   907,  909,  915,  919,  921,  929,  931,  935,  941,  943,
 /*    90 */   945,  951,  955,  958,  965,  968,  -21,  -21,  -21,  -21,
 /*   100 */   -21,  -21,  -21,  -21,  -21,   56,  209,  220,  144,  599,
 /*   110 */   -21,  630,  -21,  -21,  -21,  -21,  451,  451,  451,  405,
 /*   120 */   472,  -98,  577,  555, -101,  674,  193,  610,  677,  737,
 /*   130 */   846,  942,  946,   49,  952,  542,  534,  633,  774,  825,
 /*   140 */   748,  805,  831,  784,  806,  841,  766,  954,  476,  947,
 /*   150 */   962,  963,  969,  970,  971,  783,  621,  -79,  -19,   60,
 /*   160 */    35,  161,  155,  162,  211,  259,  256,  231,  384,  477,
 /*   170 */   481,  533,  574,  595,  644,  660,  713,  730,  819,  826,
 /*   180 */   736,  769,  896,  910,  930,  936,  944, 1031, 1035, 1045,
 /*   190 */  1047, 1048, 1049,  830,  994,  995, 1054, 1056, 1057,  256,
 /*   200 */  1058, 1059,  259, 1012, 1017, 1062, 1063, 1064, 1066, 1067,
 /*   210 */  1068, 1022, 1010, 1013,  986, 1025,  989, 1053, 1051, 1050,
 /*   220 */  1065, 1019, 1004, 1069, 1070, 1032, 1038, 1080,  998, 1026,
 /*   230 */  1071, 1052, 1072, 1073, 1074, 1075, 1014, 1021, 1095, 1076,
 /*   240 */  1037, 1079, 1081, 1077, 1096, 1084, 1100, 1103, 1092, 1089,
 /*   250 */  1097, 1098, 1101, 1102, 1106, 1105, 1111, 1104, 1113, 1115,
 /*   260 */  1078, 1083, 1085, 1090, 1091, 1107, 1109, 1112, 1120, 1121,
 /*   270 */  1123, 1125, 1130, 1131, 1132, 1135, 1137, 1136, 1140, 1141,
 /*   280 */  1138, 1149, 1151, 1156, 1157, 1159, 1160, 1164, 1150, 1152,
 /*   290 */  1143, 1145, 1153,
};
static const YYACTIONTYPE yy_default[] = {
 /*     0 */  1180, 1174, 1174, 1174, 1117, 1117, 1117, 1117, 1174, 1013,
 /*    10 */  1040, 1040, 1224, 1224, 1224, 1224, 1224, 1224, 1116, 1224,
 /*    20 */  1224, 1224, 1224, 1174, 1017, 1046, 1224, 1224, 1224, 1118,
 /*    30 */  1119, 1224, 1224, 1224, 1150, 1056, 1055, 1054, 1053, 1027,
 /*    40 */  1051, 1044, 1048, 1118, 1112, 1113, 1111, 1115, 1119, 1224,
 /*    50 */  1047, 1081, 1096, 1080, 1224, 1224, 1224, 1224, 1224, 1224,
 /*    60 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224,
 /*    70 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224,
 /*    80 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224,
 /*    90 */  1224, 1224, 1224, 1224, 1224, 1224, 1090, 1095, 1102, 1094,
 /*   100 */  1091, 1083, 1082, 1084, 1085,  984, 1224, 1224, 1224, 1224,
 /*   110 */  1086, 1224, 1087, 1099, 1098, 1097, 1172, 1189, 1188, 1224,
 /*   120 */  1224, 1224, 1224, 1124, 1224, 1224, 1224, 1224, 1224, 1224,
 /*   130 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224,
 /*   140 */  1224, 1224, 1224, 1224, 1224, 1174,  942,  942, 1224, 1174,
 /*   150 */  1174, 1174, 1174, 1174, 1174, 1017, 1008, 1224, 1224, 1224,
 /*   160 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1013, 1224, 1224,
 /*   170 */  1224, 1224, 1224, 1224, 1169, 1224, 1166, 1224, 1224, 1224,
 /*   180 */  1224, 1145, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224,
 /*   190 */  1224, 1224, 1224, 1013, 1013, 1013, 1224, 1224, 1224, 1015,
 /*   200 */  1224, 1224, 1224,  997, 1007, 1224, 1224, 1224, 1224, 1224,
 /*   210 */  1183, 1050, 1029, 1029, 1221, 1050, 1221,  959, 1202,  956,
 /*   220 */  1040, 1029, 1114, 1040, 1040, 1014, 1007, 1224, 1222, 1061,
 /*   230 */   987, 1050,  993,  993,  993,  993, 1149, 1218,  935, 1050,
 /*   240 */  1061,  987,  987, 1050,  935, 1125,  935,  935, 1125,  985,
 /*   250 */   985,  985,  974, 1125,  985,  959,  985,  974,  985,  985,
 /*   260 */  1033, 1028, 1033, 1028, 1033, 1028, 1033, 1028, 1120, 1224,
 /*   270 */  1125, 1129, 1129, 1125, 1045, 1034, 1043, 1041, 1050,  939,
 /*   280 */   977, 1186, 1186, 1182, 1182, 1182, 1182,  925, 1197, 1197,
 /*   290 */   961,  961, 1197, 1224, 1224, 1224, 1192, 1132, 1224, 1224,
 /*   300 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224,
 /*   310 */  1224, 1224, 1224, 1224, 1224, 1224, 1067, 1224,  922, 1224,
 /*   320 */  1224, 1173, 1224, 1167, 1224, 1224, 1213, 1224, 1224, 1224,
 /*   330 */  1224, 1224, 1224, 1224, 1148, 1147, 1224, 1224, 1224, 1224,
 /*   340 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1220,
 /*   350 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224,
 /*   360 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224, 1224,
 /*   370 */  1224, 1224, 1224, 1224, 1224,  999, 1224, 1224, 1224, 1206,
 /*   380 */  1224, 1224, 1224, 1224, 1224, 1224, 1224, 1042, 1224, 1035,
 /*   390 */  1224, 1224, 1210, 1224, 1224, 1224, 1224, 1224, 1224, 1224,
 /*   400 */  1224, 1224, 1224, 1224, 1176, 1224, 1224, 1224, 1175, 1224,
 /*   410 */  1224, 1224, 1224, 1224, 1224, 1068, 1224,  929, 1224, 1224,
};
/********** End of lemon-generated parsing tables *****************************/

/* The next table maps tokens (terminal symbols) into fallback tokens.  
** If a construct like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
**
** This feature can be used, for example, to cause some keywords in a language
** to revert to identifiers if they keyword does not apply in the context where
** it appears.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
    0,  /*          $ => nothing */
    0,  /*       SEMI => nothing */
   52,  /*    EXPLAIN => ID */
   52,  /*      QUERY => ID */
   52,  /*       PLAN => ID */
    0,  /*         OR => nothing */
    0,  /*        AND => nothing */
    0,  /*        NOT => nothing */
    0,  /*         IS => nothing */
   52,  /*      MATCH => ID */
   52,  /*    LIKE_KW => ID */
    0,  /*    BETWEEN => nothing */
    0,  /*         IN => nothing */
    0,  /*     ISNULL => nothing */
    0,  /*    NOTNULL => nothing */
    0,  /*         NE => nothing */
    0,  /*         EQ => nothing */
    0,  /*         GT => nothing */
    0,  /*         LE => nothing */
    0,  /*         LT => nothing */
    0,  /*         GE => nothing */
    0,  /*     ESCAPE => nothing */
    0,  /*     BITAND => nothing */
    0,  /*      BITOR => nothing */
    0,  /*     LSHIFT => nothing */
    0,  /*     RSHIFT => nothing */
    0,  /*       PLUS => nothing */
    0,  /*      MINUS => nothing */
    0,  /*       STAR => nothing */
    0,  /*      SLASH => nothing */
    0,  /*        REM => nothing */
    0,  /*     CONCAT => nothing */
    0,  /*    COLLATE => nothing */
    0,  /*     BITNOT => nothing */
   52,  /*      BEGIN => ID */
    0,  /* TRANSACTION => nothing */
   52,  /*   DEFERRED => ID */
    0,  /*     COMMIT => nothing */
   52,  /*        END => ID */
   52,  /*   ROLLBACK => ID */
   52,  /*  SAVEPOINT => ID */
   52,  /*    RELEASE => ID */
    0,  /*         TO => nothing */
    0,  /*      TABLE => nothing */
    0,  /*     CREATE => nothing */
   52,  /*         IF => ID */
    0,  /*     EXISTS => nothing */
    0,  /*         LP => nothing */
    0,  /*         RP => nothing */
    0,  /*         AS => nothing */
   52,  /*    WITHOUT => ID */
    0,  /*      COMMA => nothing */
    0,  /*         ID => nothing */
    0,  /*    INDEXED => nothing */
   52,  /*      ABORT => ID */
   52,  /*     ACTION => ID */
   52,  /*      AFTER => ID */
   52,  /*    ANALYZE => ID */
   52,  /*        ASC => ID */
   52,  /*     ATTACH => ID */
   52,  /*     BEFORE => ID */
   52,  /*         BY => ID */
   52,  /*    CASCADE => ID */
   52,  /*       CAST => ID */
   52,  /*   COLUMNKW => ID */
   52,  /*   CONFLICT => ID */
   52,  /*   DATABASE => ID */
   52,  /*       DESC => ID */
   52,  /*     DETACH => ID */
   52,  /*       EACH => ID */
   52,  /*       FAIL => ID */
   52,  /*        FOR => ID */
   52,  /*     IGNORE => ID */
   52,  /*  IMMEDIATE => ID */
   52,  /*  INITIALLY => ID */
   52,  /*    INSTEAD => ID */
   52,  /*         NO => ID */
   52,  /*        KEY => ID */
   52,  /*         OF => ID */
   52,  /*     OFFSET => ID */
   52,  /*     PRAGMA => ID */
   52,  /*      RAISE => ID */
   52,  /*  RECURSIVE => ID */
   52,  /*    REPLACE => ID */
   52,  /*   RESTRICT => ID */
   52,  /*        ROW => ID */
   52,  /*    TRIGGER => ID */
   52,  /*       VIEW => ID */
   52,  /*       WITH => ID */
   52,  /*    REINDEX => ID */
   52,  /*     RENAME => ID */
   52,  /*   CTIME_KW => ID */
};
#endif /* YYFALLBACK */

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
**
** After the "shift" half of a SHIFTREDUCE action, the stateno field
** actually contains the reduce action for the second half of the
** SHIFTREDUCE.
*/
struct yyStackEntry {
  YYACTIONTYPE stateno;  /* The state-number, or reduce action in SHIFTREDUCE */
  YYCODETYPE major;      /* The major token value.  This is the code
                         ** number for the token at this stack level */
  YYMINORTYPE minor;     /* The user-supplied minor token value.  This
                         ** is the value of the token  */
};
typedef struct yyStackEntry yyStackEntry;

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
  yyStackEntry *yytos;          /* Pointer to top element of the stack */
#ifdef YYTRACKMAXSTACKDEPTH
  int yyhwm;                    /* High-water mark of the stack */
#endif
#ifndef YYNOERRORRECOVERY
  int yyerrcnt;                 /* Shifts left before out of the error */
#endif
  sqlite3ParserARG_SDECL                /* A place to hold %extra_argument */
#if YYSTACKDEPTH<=0
  int yystksz;                  /* Current side of the stack */
  yyStackEntry *yystack;        /* The parser's stack */
  yyStackEntry yystk0;          /* First stack entry */
#else
  yyStackEntry yystack[YYSTACKDEPTH];  /* The parser's stack */
#endif
};
typedef struct yyParser yyParser;

#ifndef NDEBUG
#include <stdio.h>
static FILE *yyTraceFILE = 0;
static char *yyTracePrompt = 0;
#endif /* NDEBUG */

#ifndef NDEBUG
/* 
** Turn parser tracing on by giving a stream to which to write the trace
** and a prompt to preface each trace message.  Tracing is turned off
** by making either argument NULL 
**
** Inputs:
** <ul>
** <li> A FILE* to which trace output should be written.
**      If NULL, then tracing is turned off.
** <li> A prefix string written at the beginning of every
**      line of trace output.  If NULL, then tracing is
**      turned off.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3ParserTrace(FILE *TraceFILE, char *zTracePrompt){
  yyTraceFILE = TraceFILE;
  yyTracePrompt = zTracePrompt;
  if( yyTraceFILE==0 ) yyTracePrompt = 0;
  else if( yyTracePrompt==0 ) yyTraceFILE = 0;
}
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
static const char *const yyTokenName[] = { 
  "$",             "SEMI",          "EXPLAIN",       "QUERY",       
  "PLAN",          "OR",            "AND",           "NOT",         
  "IS",            "MATCH",         "LIKE_KW",       "BETWEEN",     
  "IN",            "ISNULL",        "NOTNULL",       "NE",          
  "EQ",            "GT",            "LE",            "LT",          
  "GE",            "ESCAPE",        "BITAND",        "BITOR",       
  "LSHIFT",        "RSHIFT",        "PLUS",          "MINUS",       
  "STAR",          "SLASH",         "REM",           "CONCAT",      
  "COLLATE",       "BITNOT",        "BEGIN",         "TRANSACTION", 
  "DEFERRED",      "COMMIT",        "END",           "ROLLBACK",    
  "SAVEPOINT",     "RELEASE",       "TO",            "TABLE",       
  "CREATE",        "IF",            "EXISTS",        "LP",          
  "RP",            "AS",            "WITHOUT",       "COMMA",       
  "ID",            "INDEXED",       "ABORT",         "ACTION",      
  "AFTER",         "ANALYZE",       "ASC",           "ATTACH",      
  "BEFORE",        "BY",            "CASCADE",       "CAST",        
  "COLUMNKW",      "CONFLICT",      "DATABASE",      "DESC",        
  "DETACH",        "EACH",          "FAIL",          "FOR",         
  "IGNORE",        "IMMEDIATE",     "INITIALLY",     "INSTEAD",     
  "NO",            "KEY",           "OF",            "OFFSET",      
  "PRAGMA",        "RAISE",         "RECURSIVE",     "REPLACE",     
  "RESTRICT",      "ROW",           "TRIGGER",       "VIEW",        
  "WITH",          "REINDEX",       "RENAME",        "CTIME_KW",    
  "ANY",           "STRING",        "JOIN_KW",       "CONSTRAINT",  
  "DEFAULT",       "NULL",          "PRIMARY",       "UNIQUE",      
  "CHECK",         "REFERENCES",    "AUTOINCR",      "ON",          
  "INSERT",        "DELETE",        "UPDATE",        "SET",         
  "DEFERRABLE",    "FOREIGN",       "DROP",          "UNION",       
  "ALL",           "EXCEPT",        "INTERSECT",     "SELECT",      
  "VALUES",        "DISTINCT",      "DOT",           "FROM",        
  "JOIN",          "USING",         "ORDER",         "GROUP",       
  "HAVING",        "LIMIT",         "WHERE",         "INTO",        
  "FLOAT",         "BLOB",          "INTEGER",       "VARIABLE",    
  "CASE",          "WHEN",          "THEN",          "ELSE",        
  "INDEX",         "ALTER",         "ADD",           "error",       
  "input",         "ecmd",          "explain",       "cmdx",        
  "cmd",           "transtype",     "trans_opt",     "nm",          
  "savepoint_opt",  "create_table",  "create_table_args",  "createkw",    
  "ifnotexists",   "columnlist",    "conslist_opt",  "table_options",
  "select",        "columnname",    "carglist",      "typetoken",   
  "typename",      "signed",        "plus_num",      "minus_num",   
  "ccons",         "term",          "expr",          "onconf",      
  "sortorder",     "autoinc",       "eidlist_opt",   "refargs",     
  "defer_subclause",  "refarg",        "refact",        "init_deferred_pred_opt",
  "conslist",      "tconscomma",    "tcons",         "sortlist",    
  "eidlist",       "defer_subclause_opt",  "orconf",        "resolvetype", 
  "raisetype",     "ifexists",      "fullname",      "selectnowith",
  "oneselect",     "with",          "multiselect_op",  "distinct",    
  "selcollist",    "from",          "where_opt",     "groupby_opt", 
  "having_opt",    "orderby_opt",   "limit_opt",     "values",      
  "nexprlist",     "exprlist",      "sclp",          "as",          
  "seltablist",    "stl_prefix",    "joinop",        "indexed_opt", 
  "on_opt",        "using_opt",     "idlist",        "setlist",     
  "insert_cmd",    "idlist_opt",    "likeop",        "between_op",  
  "in_op",         "paren_exprlist",  "case_operand",  "case_exprlist",
  "case_else",     "uniqueflag",    "collate",       "nmnum",       
  "trigger_decl",  "trigger_cmd_list",  "trigger_time",  "trigger_event",
  "foreach_clause",  "when_clause",   "trigger_cmd",   "trnm",        
  "tridxby",       "add_column_fullname",  "kwcolumn_opt",  "wqlist",      
};
#endif /* NDEBUG */

#ifndef NDEBUG
/* For tracing reduce actions, the names of all rules are required.
*/
static const char *const yyRuleName[] = {
 /*   0 */ "ecmd ::= explain cmdx SEMI",
 /*   1 */ "ecmd ::= SEMI",
 /*   2 */ "explain ::= EXPLAIN",
 /*   3 */ "explain ::= EXPLAIN QUERY PLAN",
 /*   4 */ "cmd ::= BEGIN transtype trans_opt",
 /*   5 */ "transtype ::=",
 /*   6 */ "transtype ::= DEFERRED",
 /*   7 */ "cmd ::= COMMIT trans_opt",
 /*   8 */ "cmd ::= END trans_opt",
 /*   9 */ "cmd ::= ROLLBACK trans_opt",
 /*  10 */ "cmd ::= SAVEPOINT nm",
 /*  11 */ "cmd ::= RELEASE savepoint_opt nm",
 /*  12 */ "cmd ::= ROLLBACK trans_opt TO savepoint_opt nm",
 /*  13 */ "create_table ::= createkw TABLE ifnotexists nm",
 /*  14 */ "createkw ::= CREATE",
 /*  15 */ "ifnotexists ::=",
 /*  16 */ "ifnotexists ::= IF NOT EXISTS",
 /*  17 */ "create_table_args ::= LP columnlist conslist_opt RP table_options",
 /*  18 */ "create_table_args ::= AS select",
 /*  19 */ "table_options ::=",
 /*  20 */ "table_options ::= WITHOUT nm",
 /*  21 */ "columnname ::= nm typetoken",
 /*  22 */ "typetoken ::=",
 /*  23 */ "typetoken ::= typename LP signed RP",
 /*  24 */ "typetoken ::= typename LP signed COMMA signed RP",
 /*  25 */ "typename ::= typename ID|STRING",
 /*  26 */ "ccons ::= CONSTRAINT nm",
 /*  27 */ "ccons ::= DEFAULT term",
 /*  28 */ "ccons ::= DEFAULT LP expr RP",
 /*  29 */ "ccons ::= DEFAULT PLUS term",
 /*  30 */ "ccons ::= DEFAULT MINUS term",
 /*  31 */ "ccons ::= DEFAULT ID|INDEXED",
 /*  32 */ "ccons ::= NOT NULL onconf",
 /*  33 */ "ccons ::= PRIMARY KEY sortorder onconf autoinc",
 /*  34 */ "ccons ::= UNIQUE onconf",
 /*  35 */ "ccons ::= CHECK LP expr RP",
 /*  36 */ "ccons ::= REFERENCES nm eidlist_opt refargs",
 /*  37 */ "ccons ::= defer_subclause",
 /*  38 */ "ccons ::= COLLATE ID|STRING",
 /*  39 */ "autoinc ::=",
 /*  40 */ "autoinc ::= AUTOINCR",
 /*  41 */ "refargs ::=",
 /*  42 */ "refargs ::= refargs refarg",
 /*  43 */ "refarg ::= MATCH nm",
 /*  44 */ "refarg ::= ON INSERT refact",
 /*  45 */ "refarg ::= ON DELETE refact",
 /*  46 */ "refarg ::= ON UPDATE refact",
 /*  47 */ "refact ::= SET NULL",
 /*  48 */ "refact ::= SET DEFAULT",
 /*  49 */ "refact ::= CASCADE",
 /*  50 */ "refact ::= RESTRICT",
 /*  51 */ "refact ::= NO ACTION",
 /*  52 */ "defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt",
 /*  53 */ "defer_subclause ::= DEFERRABLE init_deferred_pred_opt",
 /*  54 */ "init_deferred_pred_opt ::=",
 /*  55 */ "init_deferred_pred_opt ::= INITIALLY DEFERRED",
 /*  56 */ "init_deferred_pred_opt ::= INITIALLY IMMEDIATE",
 /*  57 */ "conslist_opt ::=",
 /*  58 */ "tconscomma ::= COMMA",
 /*  59 */ "tcons ::= CONSTRAINT nm",
 /*  60 */ "tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf",
 /*  61 */ "tcons ::= UNIQUE LP sortlist RP onconf",
 /*  62 */ "tcons ::= CHECK LP expr RP onconf",
 /*  63 */ "tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt",
 /*  64 */ "defer_subclause_opt ::=",
 /*  65 */ "onconf ::=",
 /*  66 */ "onconf ::= ON CONFLICT resolvetype",
 /*  67 */ "orconf ::=",
 /*  68 */ "orconf ::= OR resolvetype",
 /*  69 */ "resolvetype ::= IGNORE",
 /*  70 */ "resolvetype ::= REPLACE",
 /*  71 */ "cmd ::= DROP TABLE ifexists fullname",
 /*  72 */ "ifexists ::= IF EXISTS",
 /*  73 */ "ifexists ::=",
 /*  74 */ "cmd ::= createkw VIEW ifnotexists nm eidlist_opt AS select",
 /*  75 */ "cmd ::= DROP VIEW ifexists fullname",
 /*  76 */ "cmd ::= select",
 /*  77 */ "select ::= with selectnowith",
 /*  78 */ "selectnowith ::= selectnowith multiselect_op oneselect",
 /*  79 */ "multiselect_op ::= UNION",
 /*  80 */ "multiselect_op ::= UNION ALL",
 /*  81 */ "multiselect_op ::= EXCEPT|INTERSECT",
 /*  82 */ "oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt",
 /*  83 */ "values ::= VALUES LP nexprlist RP",
 /*  84 */ "values ::= values COMMA LP exprlist RP",
 /*  85 */ "distinct ::= DISTINCT",
 /*  86 */ "distinct ::= ALL",
 /*  87 */ "distinct ::=",
 /*  88 */ "sclp ::=",
 /*  89 */ "selcollist ::= sclp expr as",
 /*  90 */ "selcollist ::= sclp STAR",
 /*  91 */ "selcollist ::= sclp nm DOT STAR",
 /*  92 */ "as ::= AS nm",
 /*  93 */ "as ::=",
 /*  94 */ "from ::=",
 /*  95 */ "from ::= FROM seltablist",
 /*  96 */ "stl_prefix ::= seltablist joinop",
 /*  97 */ "stl_prefix ::=",
 /*  98 */ "seltablist ::= stl_prefix nm as indexed_opt on_opt using_opt",
 /*  99 */ "seltablist ::= stl_prefix nm LP exprlist RP as on_opt using_opt",
 /* 100 */ "seltablist ::= stl_prefix LP select RP as on_opt using_opt",
 /* 101 */ "seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt",
 /* 102 */ "fullname ::= nm",
 /* 103 */ "joinop ::= COMMA|JOIN",
 /* 104 */ "joinop ::= JOIN_KW JOIN",
 /* 105 */ "joinop ::= JOIN_KW nm JOIN",
 /* 106 */ "joinop ::= JOIN_KW nm nm JOIN",
 /* 107 */ "on_opt ::= ON expr",
 /* 108 */ "on_opt ::=",
 /* 109 */ "indexed_opt ::=",
 /* 110 */ "indexed_opt ::= INDEXED BY nm",
 /* 111 */ "indexed_opt ::= NOT INDEXED",
 /* 112 */ "using_opt ::= USING LP idlist RP",
 /* 113 */ "using_opt ::=",
 /* 114 */ "orderby_opt ::=",
 /* 115 */ "orderby_opt ::= ORDER BY sortlist",
 /* 116 */ "sortlist ::= sortlist COMMA expr sortorder",
 /* 117 */ "sortlist ::= expr sortorder",
 /* 118 */ "sortorder ::= ASC",
 /* 119 */ "sortorder ::= DESC",
 /* 120 */ "sortorder ::=",
 /* 121 */ "groupby_opt ::=",
 /* 122 */ "groupby_opt ::= GROUP BY nexprlist",
 /* 123 */ "having_opt ::=",
 /* 124 */ "having_opt ::= HAVING expr",
 /* 125 */ "limit_opt ::=",
 /* 126 */ "limit_opt ::= LIMIT expr",
 /* 127 */ "limit_opt ::= LIMIT expr OFFSET expr",
 /* 128 */ "limit_opt ::= LIMIT expr COMMA expr",
 /* 129 */ "cmd ::= with DELETE FROM fullname indexed_opt where_opt",
 /* 130 */ "where_opt ::=",
 /* 131 */ "where_opt ::= WHERE expr",
 /* 132 */ "cmd ::= with UPDATE orconf fullname indexed_opt SET setlist where_opt",
 /* 133 */ "setlist ::= setlist COMMA nm EQ expr",
 /* 134 */ "setlist ::= setlist COMMA LP idlist RP EQ expr",
 /* 135 */ "setlist ::= nm EQ expr",
 /* 136 */ "setlist ::= LP idlist RP EQ expr",
 /* 137 */ "cmd ::= with insert_cmd INTO fullname idlist_opt select",
 /* 138 */ "cmd ::= with insert_cmd INTO fullname idlist_opt DEFAULT VALUES",
 /* 139 */ "insert_cmd ::= INSERT orconf",
 /* 140 */ "insert_cmd ::= REPLACE",
 /* 141 */ "idlist_opt ::=",
 /* 142 */ "idlist_opt ::= LP idlist RP",
 /* 143 */ "idlist ::= idlist COMMA nm",
 /* 144 */ "idlist ::= nm",
 /* 145 */ "expr ::= LP expr RP",
 /* 146 */ "term ::= NULL",
 /* 147 */ "expr ::= ID|INDEXED",
 /* 148 */ "expr ::= JOIN_KW",
 /* 149 */ "expr ::= nm DOT nm",
 /* 150 */ "term ::= FLOAT|BLOB",
 /* 151 */ "term ::= STRING",
 /* 152 */ "term ::= INTEGER",
 /* 153 */ "expr ::= VARIABLE",
 /* 154 */ "expr ::= expr COLLATE ID|STRING",
 /* 155 */ "expr ::= CAST LP expr AS typetoken RP",
 /* 156 */ "expr ::= ID|INDEXED LP distinct exprlist RP",
 /* 157 */ "expr ::= ID|INDEXED LP STAR RP",
 /* 158 */ "term ::= CTIME_KW",
 /* 159 */ "expr ::= LP nexprlist COMMA expr RP",
 /* 160 */ "expr ::= expr AND expr",
 /* 161 */ "expr ::= expr OR expr",
 /* 162 */ "expr ::= expr LT|GT|GE|LE expr",
 /* 163 */ "expr ::= expr EQ|NE expr",
 /* 164 */ "expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr",
 /* 165 */ "expr ::= expr PLUS|MINUS expr",
 /* 166 */ "expr ::= expr STAR|SLASH|REM expr",
 /* 167 */ "expr ::= expr CONCAT expr",
 /* 168 */ "likeop ::= LIKE_KW|MATCH",
 /* 169 */ "likeop ::= NOT LIKE_KW|MATCH",
 /* 170 */ "expr ::= expr likeop expr",
 /* 171 */ "expr ::= expr likeop expr ESCAPE expr",
 /* 172 */ "expr ::= expr ISNULL|NOTNULL",
 /* 173 */ "expr ::= expr NOT NULL",
 /* 174 */ "expr ::= expr IS expr",
 /* 175 */ "expr ::= expr IS NOT expr",
 /* 176 */ "expr ::= NOT expr",
 /* 177 */ "expr ::= BITNOT expr",
 /* 178 */ "expr ::= MINUS expr",
 /* 179 */ "expr ::= PLUS expr",
 /* 180 */ "between_op ::= BETWEEN",
 /* 181 */ "between_op ::= NOT BETWEEN",
 /* 182 */ "expr ::= expr between_op expr AND expr",
 /* 183 */ "in_op ::= IN",
 /* 184 */ "in_op ::= NOT IN",
 /* 185 */ "expr ::= expr in_op LP exprlist RP",
 /* 186 */ "expr ::= LP select RP",
 /* 187 */ "expr ::= expr in_op LP select RP",
 /* 188 */ "expr ::= expr in_op nm paren_exprlist",
 /* 189 */ "expr ::= EXISTS LP select RP",
 /* 190 */ "expr ::= CASE case_operand case_exprlist case_else END",
 /* 191 */ "case_exprlist ::= case_exprlist WHEN expr THEN expr",
 /* 192 */ "case_exprlist ::= WHEN expr THEN expr",
 /* 193 */ "case_else ::= ELSE expr",
 /* 194 */ "case_else ::=",
 /* 195 */ "case_operand ::= expr",
 /* 196 */ "case_operand ::=",
 /* 197 */ "exprlist ::=",
 /* 198 */ "nexprlist ::= nexprlist COMMA expr",
 /* 199 */ "nexprlist ::= expr",
 /* 200 */ "paren_exprlist ::=",
 /* 201 */ "paren_exprlist ::= LP exprlist RP",
 /* 202 */ "cmd ::= createkw uniqueflag INDEX ifnotexists nm ON nm LP sortlist RP where_opt",
 /* 203 */ "uniqueflag ::= UNIQUE",
 /* 204 */ "uniqueflag ::=",
 /* 205 */ "eidlist_opt ::=",
 /* 206 */ "eidlist_opt ::= LP eidlist RP",
 /* 207 */ "eidlist ::= eidlist COMMA nm collate sortorder",
 /* 208 */ "eidlist ::= nm collate sortorder",
 /* 209 */ "collate ::=",
 /* 210 */ "collate ::= COLLATE ID|STRING",
 /* 211 */ "cmd ::= DROP INDEX ifexists fullname ON nm",
 /* 212 */ "cmd ::= PRAGMA nm",
 /* 213 */ "cmd ::= PRAGMA nm EQ nmnum",
 /* 214 */ "cmd ::= PRAGMA nm LP nmnum RP",
 /* 215 */ "cmd ::= PRAGMA nm EQ minus_num",
 /* 216 */ "cmd ::= PRAGMA nm LP minus_num RP",
 /* 217 */ "cmd ::= PRAGMA nm EQ nm DOT nm",
 /* 218 */ "plus_num ::= PLUS INTEGER|FLOAT",
 /* 219 */ "minus_num ::= MINUS INTEGER|FLOAT",
 /* 220 */ "cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END",
 /* 221 */ "trigger_decl ::= TRIGGER ifnotexists nm trigger_time trigger_event ON fullname foreach_clause when_clause",
 /* 222 */ "trigger_time ::= BEFORE",
 /* 223 */ "trigger_time ::= AFTER",
 /* 224 */ "trigger_time ::= INSTEAD OF",
 /* 225 */ "trigger_time ::=",
 /* 226 */ "trigger_event ::= DELETE|INSERT",
 /* 227 */ "trigger_event ::= UPDATE",
 /* 228 */ "trigger_event ::= UPDATE OF idlist",
 /* 229 */ "when_clause ::=",
 /* 230 */ "when_clause ::= WHEN expr",
 /* 231 */ "trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI",
 /* 232 */ "trigger_cmd_list ::= trigger_cmd SEMI",
 /* 233 */ "trnm ::= nm DOT nm",
 /* 234 */ "tridxby ::= INDEXED BY nm",
 /* 235 */ "tridxby ::= NOT INDEXED",
 /* 236 */ "trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt",
 /* 237 */ "trigger_cmd ::= insert_cmd INTO trnm idlist_opt select",
 /* 238 */ "trigger_cmd ::= DELETE FROM trnm tridxby where_opt",
 /* 239 */ "trigger_cmd ::= select",
 /* 240 */ "expr ::= RAISE LP IGNORE RP",
 /* 241 */ "expr ::= RAISE LP raisetype COMMA STRING RP",
 /* 242 */ "raisetype ::= ROLLBACK",
 /* 243 */ "raisetype ::= ABORT",
 /* 244 */ "raisetype ::= FAIL",
 /* 245 */ "cmd ::= DROP TRIGGER ifexists fullname",
 /* 246 */ "cmd ::= REINDEX",
 /* 247 */ "cmd ::= REINDEX nm",
 /* 248 */ "cmd ::= REINDEX nm ON nm",
 /* 249 */ "cmd ::= ANALYZE",
 /* 250 */ "cmd ::= ANALYZE nm",
 /* 251 */ "cmd ::= ALTER TABLE fullname RENAME TO nm",
 /* 252 */ "cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist",
 /* 253 */ "add_column_fullname ::= fullname",
 /* 254 */ "with ::=",
 /* 255 */ "with ::= WITH wqlist",
 /* 256 */ "with ::= WITH RECURSIVE wqlist",
 /* 257 */ "wqlist ::= nm eidlist_opt AS LP select RP",
 /* 258 */ "wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP",
 /* 259 */ "input ::= ecmd",
 /* 260 */ "explain ::=",
 /* 261 */ "cmdx ::= cmd",
 /* 262 */ "trans_opt ::=",
 /* 263 */ "trans_opt ::= TRANSACTION",
 /* 264 */ "trans_opt ::= TRANSACTION nm",
 /* 265 */ "savepoint_opt ::= SAVEPOINT",
 /* 266 */ "savepoint_opt ::=",
 /* 267 */ "cmd ::= create_table create_table_args",
 /* 268 */ "columnlist ::= columnlist COMMA columnname carglist",
 /* 269 */ "columnlist ::= columnname carglist",
 /* 270 */ "nm ::= ID|INDEXED",
 /* 271 */ "nm ::= JOIN_KW",
 /* 272 */ "typetoken ::= typename",
 /* 273 */ "typename ::= ID|STRING",
 /* 274 */ "signed ::= plus_num",
 /* 275 */ "signed ::= minus_num",
 /* 276 */ "carglist ::= carglist ccons",
 /* 277 */ "carglist ::=",
 /* 278 */ "ccons ::= NULL onconf",
 /* 279 */ "conslist_opt ::= COMMA conslist",
 /* 280 */ "conslist ::= conslist tconscomma tcons",
 /* 281 */ "conslist ::= tcons",
 /* 282 */ "tconscomma ::=",
 /* 283 */ "defer_subclause_opt ::= defer_subclause",
 /* 284 */ "resolvetype ::= raisetype",
 /* 285 */ "selectnowith ::= oneselect",
 /* 286 */ "oneselect ::= values",
 /* 287 */ "sclp ::= selcollist COMMA",
 /* 288 */ "as ::= ID|STRING",
 /* 289 */ "expr ::= term",
 /* 290 */ "exprlist ::= nexprlist",
 /* 291 */ "nmnum ::= plus_num",
 /* 292 */ "nmnum ::= STRING",
 /* 293 */ "nmnum ::= nm",
 /* 294 */ "nmnum ::= ON",
 /* 295 */ "nmnum ::= DELETE",
 /* 296 */ "nmnum ::= DEFAULT",
 /* 297 */ "plus_num ::= INTEGER|FLOAT",
 /* 298 */ "foreach_clause ::=",
 /* 299 */ "foreach_clause ::= FOR EACH ROW",
 /* 300 */ "trnm ::= nm",
 /* 301 */ "tridxby ::=",
 /* 302 */ "kwcolumn_opt ::=",
 /* 303 */ "kwcolumn_opt ::= COLUMNKW",
};
#endif /* NDEBUG */


#if YYSTACKDEPTH<=0
/*
** Try to increase the size of the parser stack.  Return the number
** of errors.  Return 0 on success.
*/
static int yyGrowStack(yyParser *p){
  int newSize;
  int idx;
  yyStackEntry *pNew;

  newSize = p->yystksz*2 + 100;
  idx = p->yytos ? (int)(p->yytos - p->yystack) : 0;
  if( p->yystack==&p->yystk0 ){
    pNew = malloc(newSize*sizeof(pNew[0]));
    if( pNew ) pNew[0] = p->yystk0;
  }else{
    pNew = realloc(p->yystack, newSize*sizeof(pNew[0]));
  }
  if( pNew ){
    p->yystack = pNew;
    p->yytos = &p->yystack[idx];
#ifndef NDEBUG
    if( yyTraceFILE ){
      fprintf(yyTraceFILE,"%sStack grows from %d to %d entries.\n",
              yyTracePrompt, p->yystksz, newSize);
    }
#endif
    p->yystksz = newSize;
  }
  return pNew==0; 
}
#endif

/* Datatype of the argument to the memory allocated passed as the
** second argument to sqlite3ParserAlloc() below.  This can be changed by
** putting an appropriate #define in the %include section of the input
** grammar.
*/
#ifndef YYMALLOCARGTYPE
# define YYMALLOCARGTYPE size_t
#endif

/* 
** This function allocates a new parser.
** The only argument is a pointer to a function which works like
** malloc.
**
** Inputs:
** A pointer to the function used to allocate memory.
**
** Outputs:
** A pointer to a parser.  This pointer is used in subsequent calls
** to sqlite3Parser and sqlite3ParserFree.
*/
void *sqlite3ParserAlloc(void *(*mallocProc)(YYMALLOCARGTYPE)){
  yyParser *pParser;
  pParser = (yyParser*)(*mallocProc)( (YYMALLOCARGTYPE)sizeof(yyParser) );
  if( pParser ){
#ifdef YYTRACKMAXSTACKDEPTH
    pParser->yyhwm = 0;
#endif
#if YYSTACKDEPTH<=0
    pParser->yytos = NULL;
    pParser->yystack = NULL;
    pParser->yystksz = 0;
    if( yyGrowStack(pParser) ){
      pParser->yystack = &pParser->yystk0;
      pParser->yystksz = 1;
    }
#endif
#ifndef YYNOERRORRECOVERY
    pParser->yyerrcnt = -1;
#endif
    pParser->yytos = pParser->yystack;
    pParser->yystack[0].stateno = 0;
    pParser->yystack[0].major = 0;
  }
  return pParser;
}

/* The following function deletes the "minor type" or semantic value
** associated with a symbol.  The symbol can be either a terminal
** or nonterminal. "yymajor" is the symbol code, and "yypminor" is
** a pointer to the value to be deleted.  The code used to do the 
** deletions is derived from the %destructor and/or %token_destructor
** directives of the input grammar.
*/
static void yy_destructor(
  yyParser *yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  YYMINORTYPE *yypminor   /* The object to be destroyed */
){
  sqlite3ParserARG_FETCH;
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are *not* used
    ** inside the C code.
    */
/********* Begin destructor definitions ***************************************/
    case 156: /* select */
    case 187: /* selectnowith */
    case 188: /* oneselect */
    case 199: /* values */
{
#line 390 "parse.y"
sqlite3SelectDelete(pParse->db, (yypminor->yy387));
#line 1500 "parse.c"
}
      break;
    case 165: /* term */
    case 166: /* expr */
{
#line 831 "parse.y"
sqlite3ExprDelete(pParse->db, (yypminor->yy326).pExpr);
#line 1508 "parse.c"
}
      break;
    case 170: /* eidlist_opt */
    case 179: /* sortlist */
    case 180: /* eidlist */
    case 192: /* selcollist */
    case 195: /* groupby_opt */
    case 197: /* orderby_opt */
    case 200: /* nexprlist */
    case 201: /* exprlist */
    case 202: /* sclp */
    case 211: /* setlist */
    case 217: /* paren_exprlist */
    case 219: /* case_exprlist */
{
#line 1264 "parse.y"
sqlite3ExprListDelete(pParse->db, (yypminor->yy258));
#line 1526 "parse.c"
}
      break;
    case 186: /* fullname */
    case 193: /* from */
    case 204: /* seltablist */
    case 205: /* stl_prefix */
{
#line 618 "parse.y"
sqlite3SrcListDelete(pParse->db, (yypminor->yy3));
#line 1536 "parse.c"
}
      break;
    case 189: /* with */
    case 235: /* wqlist */
{
#line 1509 "parse.y"
sqlite3WithDelete(pParse->db, (yypminor->yy211));
#line 1544 "parse.c"
}
      break;
    case 194: /* where_opt */
    case 196: /* having_opt */
    case 208: /* on_opt */
    case 218: /* case_operand */
    case 220: /* case_else */
    case 229: /* when_clause */
{
#line 740 "parse.y"
sqlite3ExprDelete(pParse->db, (yypminor->yy402));
#line 1556 "parse.c"
}
      break;
    case 209: /* using_opt */
    case 210: /* idlist */
    case 213: /* idlist_opt */
{
#line 652 "parse.y"
sqlite3IdListDelete(pParse->db, (yypminor->yy400));
#line 1565 "parse.c"
}
      break;
    case 225: /* trigger_cmd_list */
    case 230: /* trigger_cmd */
{
#line 1384 "parse.y"
sqlite3DeleteTriggerStep(pParse->db, (yypminor->yy19));
#line 1573 "parse.c"
}
      break;
    case 227: /* trigger_event */
{
#line 1370 "parse.y"
sqlite3IdListDelete(pParse->db, (yypminor->yy466).b);
#line 1580 "parse.c"
}
      break;
/********* End destructor definitions *****************************************/
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
*/
static void yy_pop_parser_stack(yyParser *pParser){
  yyStackEntry *yytos;
  assert( pParser->yytos!=0 );
  assert( pParser->yytos > pParser->yystack );
  yytos = pParser->yytos--;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sPopping %s\n",
      yyTracePrompt,
      yyTokenName[yytos->major]);
  }
#endif
  yy_destructor(pParser, yytos->major, &yytos->minor);
}

/* 
** Deallocate and destroy a parser.  Destructors are called for
** all stack elements before shutting the parser down.
**
** If the YYPARSEFREENEVERNULL macro exists (for example because it
** is defined in a %include section of the input grammar) then it is
** assumed that the input pointer is never NULL.
*/
void sqlite3ParserFree(
  void *p,                    /* The parser to be deleted */
  void (*freeProc)(void*)     /* Function used to reclaim memory */
){
  yyParser *pParser = (yyParser*)p;
#ifndef YYPARSEFREENEVERNULL
  if( pParser==0 ) return;
#endif
  while( pParser->yytos>pParser->yystack ) yy_pop_parser_stack(pParser);
#if YYSTACKDEPTH<=0
  if( pParser->yystack!=&pParser->yystk0 ) free(pParser->yystack);
#endif
  (*freeProc)((void*)pParser);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int sqlite3ParserStackPeak(void *p){
  yyParser *pParser = (yyParser*)p;
  return pParser->yyhwm;
}
#endif

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
*/
static unsigned int yy_find_shift_action(
  yyParser *pParser,        /* The parser */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
  int stateno = pParser->yytos->stateno;
 
  if( stateno>=YY_MIN_REDUCE ) return stateno;
  assert( stateno <= YY_SHIFT_COUNT );
  do{
    i = yy_shift_ofst[stateno];
    assert( iLookAhead!=YYNOCODE );
    i += iLookAhead;
    if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token */
      if( iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0])
             && (iFallback = yyFallback[iLookAhead])!=0 ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE, "%sFALLBACK %s => %s\n",
             yyTracePrompt, yyTokenName[iLookAhead], yyTokenName[iFallback]);
        }
#endif
        assert( yyFallback[iFallback]==0 ); /* Fallback loop must terminate */
        iLookAhead = iFallback;
        continue;
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        if( 
#if YY_SHIFT_MIN+YYWILDCARD<0
          j>=0 &&
#endif
#if YY_SHIFT_MAX+YYWILDCARD>=YY_ACTTAB_COUNT
          j<YY_ACTTAB_COUNT &&
#endif
          yy_lookahead[j]==YYWILDCARD && iLookAhead>0
        ){
#ifndef NDEBUG
          if( yyTraceFILE ){
            fprintf(yyTraceFILE, "%sWILDCARD %s => %s\n",
               yyTracePrompt, yyTokenName[iLookAhead],
               yyTokenName[YYWILDCARD]);
          }
#endif /* NDEBUG */
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
      return yy_default[stateno];
    }else{
      return yy_action[i];
    }
  }while(1);
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
*/
static int yy_find_reduce_action(
  int stateno,              /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert( stateno<=YY_REDUCE_COUNT );
#endif
  i = yy_reduce_ofst[stateno];
  assert( i!=YY_REDUCE_USE_DFLT );
  assert( iLookAhead!=YYNOCODE );
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert( i>=0 && i<YY_ACTTAB_COUNT );
  assert( yy_lookahead[i]==iLookAhead );
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser *yypParser){
   sqlite3ParserARG_FETCH;
#ifndef NDEBUG
   if( yyTraceFILE ){
     fprintf(yyTraceFILE,"%sStack Overflow!\n",yyTracePrompt);
   }
#endif
   while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
/******** Begin %stack_overflow code ******************************************/
#line 37 "parse.y"

  sqlite3ErrorMsg(pParse, "parser stack overflow");
#line 1753 "parse.c"
/******** End %stack_overflow code ********************************************/
   sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument var */
}

/*
** Print tracing information for a SHIFT action
*/
#ifndef NDEBUG
static void yyTraceShift(yyParser *yypParser, int yyNewState){
  if( yyTraceFILE ){
    if( yyNewState<YYNSTATE ){
      fprintf(yyTraceFILE,"%sShift '%s', go to state %d\n",
         yyTracePrompt,yyTokenName[yypParser->yytos->major],
         yyNewState);
    }else{
      fprintf(yyTraceFILE,"%sShift '%s'\n",
         yyTracePrompt,yyTokenName[yypParser->yytos->major]);
    }
  }
}
#else
# define yyTraceShift(X,Y)
#endif

/*
** Perform a shift action.
*/
static void yy_shift(
  yyParser *yypParser,          /* The parser to be shifted */
  int yyNewState,               /* The new state to shift in */
  int yyMajor,                  /* The major token to shift in */
  sqlite3ParserTOKENTYPE yyMinor        /* The minor token to shift in */
){
  yyStackEntry *yytos;
  yypParser->yytos++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
    yypParser->yyhwm++;
    assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack) );
  }
#endif
#if YYSTACKDEPTH>0 
  if( yypParser->yytos>=&yypParser->yystack[YYSTACKDEPTH] ){
    yypParser->yytos--;
    yyStackOverflow(yypParser);
    return;
  }
#else
  if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz] ){
    if( yyGrowStack(yypParser) ){
      yypParser->yytos--;
      yyStackOverflow(yypParser);
      return;
    }
  }
#endif
  if( yyNewState > YY_MAX_SHIFT ){
    yyNewState += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
  }
  yytos = yypParser->yytos;
  yytos->stateno = (YYACTIONTYPE)yyNewState;
  yytos->major = (YYCODETYPE)yyMajor;
  yytos->minor.yy0 = yyMinor;
  yyTraceShift(yypParser, yyNewState);
}

/* The following table contains information about every rule that
** is used during the reduce.
*/
static const struct {
  YYCODETYPE lhs;         /* Symbol on the left-hand side of the rule */
  unsigned char nrhs;     /* Number of right-hand side symbols in the rule */
} yyRuleInfo[] = {
  { 141, 3 },
  { 141, 1 },
  { 142, 1 },
  { 142, 3 },
  { 144, 3 },
  { 145, 0 },
  { 145, 1 },
  { 144, 2 },
  { 144, 2 },
  { 144, 2 },
  { 144, 2 },
  { 144, 3 },
  { 144, 5 },
  { 149, 4 },
  { 151, 1 },
  { 152, 0 },
  { 152, 3 },
  { 150, 5 },
  { 150, 2 },
  { 155, 0 },
  { 155, 2 },
  { 157, 2 },
  { 159, 0 },
  { 159, 4 },
  { 159, 6 },
  { 160, 2 },
  { 164, 2 },
  { 164, 2 },
  { 164, 4 },
  { 164, 3 },
  { 164, 3 },
  { 164, 2 },
  { 164, 3 },
  { 164, 5 },
  { 164, 2 },
  { 164, 4 },
  { 164, 4 },
  { 164, 1 },
  { 164, 2 },
  { 169, 0 },
  { 169, 1 },
  { 171, 0 },
  { 171, 2 },
  { 173, 2 },
  { 173, 3 },
  { 173, 3 },
  { 173, 3 },
  { 174, 2 },
  { 174, 2 },
  { 174, 1 },
  { 174, 1 },
  { 174, 2 },
  { 172, 3 },
  { 172, 2 },
  { 175, 0 },
  { 175, 2 },
  { 175, 2 },
  { 154, 0 },
  { 177, 1 },
  { 178, 2 },
  { 178, 7 },
  { 178, 5 },
  { 178, 5 },
  { 178, 10 },
  { 181, 0 },
  { 167, 0 },
  { 167, 3 },
  { 182, 0 },
  { 182, 2 },
  { 183, 1 },
  { 183, 1 },
  { 144, 4 },
  { 185, 2 },
  { 185, 0 },
  { 144, 7 },
  { 144, 4 },
  { 144, 1 },
  { 156, 2 },
  { 187, 3 },
  { 190, 1 },
  { 190, 2 },
  { 190, 1 },
  { 188, 9 },
  { 199, 4 },
  { 199, 5 },
  { 191, 1 },
  { 191, 1 },
  { 191, 0 },
  { 202, 0 },
  { 192, 3 },
  { 192, 2 },
  { 192, 4 },
  { 203, 2 },
  { 203, 0 },
  { 193, 0 },
  { 193, 2 },
  { 205, 2 },
  { 205, 0 },
  { 204, 6 },
  { 204, 8 },
  { 204, 7 },
  { 204, 7 },
  { 186, 1 },
  { 206, 1 },
  { 206, 2 },
  { 206, 3 },
  { 206, 4 },
  { 208, 2 },
  { 208, 0 },
  { 207, 0 },
  { 207, 3 },
  { 207, 2 },
  { 209, 4 },
  { 209, 0 },
  { 197, 0 },
  { 197, 3 },
  { 179, 4 },
  { 179, 2 },
  { 168, 1 },
  { 168, 1 },
  { 168, 0 },
  { 195, 0 },
  { 195, 3 },
  { 196, 0 },
  { 196, 2 },
  { 198, 0 },
  { 198, 2 },
  { 198, 4 },
  { 198, 4 },
  { 144, 6 },
  { 194, 0 },
  { 194, 2 },
  { 144, 8 },
  { 211, 5 },
  { 211, 7 },
  { 211, 3 },
  { 211, 5 },
  { 144, 6 },
  { 144, 7 },
  { 212, 2 },
  { 212, 1 },
  { 213, 0 },
  { 213, 3 },
  { 210, 3 },
  { 210, 1 },
  { 166, 3 },
  { 165, 1 },
  { 166, 1 },
  { 166, 1 },
  { 166, 3 },
  { 165, 1 },
  { 165, 1 },
  { 165, 1 },
  { 166, 1 },
  { 166, 3 },
  { 166, 6 },
  { 166, 5 },
  { 166, 4 },
  { 165, 1 },
  { 166, 5 },
  { 166, 3 },
  { 166, 3 },
  { 166, 3 },
  { 166, 3 },
  { 166, 3 },
  { 166, 3 },
  { 166, 3 },
  { 166, 3 },
  { 214, 1 },
  { 214, 2 },
  { 166, 3 },
  { 166, 5 },
  { 166, 2 },
  { 166, 3 },
  { 166, 3 },
  { 166, 4 },
  { 166, 2 },
  { 166, 2 },
  { 166, 2 },
  { 166, 2 },
  { 215, 1 },
  { 215, 2 },
  { 166, 5 },
  { 216, 1 },
  { 216, 2 },
  { 166, 5 },
  { 166, 3 },
  { 166, 5 },
  { 166, 4 },
  { 166, 4 },
  { 166, 5 },
  { 219, 5 },
  { 219, 4 },
  { 220, 2 },
  { 220, 0 },
  { 218, 1 },
  { 218, 0 },
  { 201, 0 },
  { 200, 3 },
  { 200, 1 },
  { 217, 0 },
  { 217, 3 },
  { 144, 11 },
  { 221, 1 },
  { 221, 0 },
  { 170, 0 },
  { 170, 3 },
  { 180, 5 },
  { 180, 3 },
  { 222, 0 },
  { 222, 2 },
  { 144, 6 },
  { 144, 2 },
  { 144, 4 },
  { 144, 5 },
  { 144, 4 },
  { 144, 5 },
  { 144, 6 },
  { 162, 2 },
  { 163, 2 },
  { 144, 5 },
  { 224, 9 },
  { 226, 1 },
  { 226, 1 },
  { 226, 2 },
  { 226, 0 },
  { 227, 1 },
  { 227, 1 },
  { 227, 3 },
  { 229, 0 },
  { 229, 2 },
  { 225, 3 },
  { 225, 2 },
  { 231, 3 },
  { 232, 3 },
  { 232, 2 },
  { 230, 7 },
  { 230, 5 },
  { 230, 5 },
  { 230, 1 },
  { 166, 4 },
  { 166, 6 },
  { 184, 1 },
  { 184, 1 },
  { 184, 1 },
  { 144, 4 },
  { 144, 1 },
  { 144, 2 },
  { 144, 4 },
  { 144, 1 },
  { 144, 2 },
  { 144, 6 },
  { 144, 7 },
  { 233, 1 },
  { 189, 0 },
  { 189, 2 },
  { 189, 3 },
  { 235, 6 },
  { 235, 8 },
  { 140, 1 },
  { 142, 0 },
  { 143, 1 },
  { 146, 0 },
  { 146, 1 },
  { 146, 2 },
  { 148, 1 },
  { 148, 0 },
  { 144, 2 },
  { 153, 4 },
  { 153, 2 },
  { 147, 1 },
  { 147, 1 },
  { 159, 1 },
  { 160, 1 },
  { 161, 1 },
  { 161, 1 },
  { 158, 2 },
  { 158, 0 },
  { 164, 2 },
  { 154, 2 },
  { 176, 3 },
  { 176, 1 },
  { 177, 0 },
  { 181, 1 },
  { 183, 1 },
  { 187, 1 },
  { 188, 1 },
  { 202, 2 },
  { 203, 1 },
  { 166, 1 },
  { 201, 1 },
  { 223, 1 },
  { 223, 1 },
  { 223, 1 },
  { 223, 1 },
  { 223, 1 },
  { 223, 1 },
  { 162, 1 },
  { 228, 0 },
  { 228, 3 },
  { 231, 1 },
  { 232, 0 },
  { 234, 0 },
  { 234, 1 },
};

static void yy_accept(yyParser*);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
*/
static void yy_reduce(
  yyParser *yypParser,         /* The parser */
  unsigned int yyruleno        /* Number of the rule by which to reduce */
){
  int yygoto;                     /* The next state */
  int yyact;                      /* The next action */
  yyStackEntry *yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  sqlite3ParserARG_FETCH;
  yymsp = yypParser->yytos;
#ifndef NDEBUG
  if( yyTraceFILE && yyruleno<(int)(sizeof(yyRuleName)/sizeof(yyRuleName[0])) ){
    yysize = yyRuleInfo[yyruleno].nrhs;
    fprintf(yyTraceFILE, "%sReduce [%s], go to state %d.\n", yyTracePrompt,
      yyRuleName[yyruleno], yymsp[-yysize].stateno);
  }
#endif /* NDEBUG */

  /* Check that the stack is large enough to grow by a single entry
  ** if the RHS of the rule is empty.  This ensures that there is room
  ** enough on the stack to push the LHS value */
  if( yyRuleInfo[yyruleno].nrhs==0 ){
#ifdef YYTRACKMAXSTACKDEPTH
    if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
      yypParser->yyhwm++;
      assert( yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack));
    }
#endif
#if YYSTACKDEPTH>0 
    if( yypParser->yytos>=&yypParser->yystack[YYSTACKDEPTH-1] ){
      yyStackOverflow(yypParser);
      return;
    }
#else
    if( yypParser->yytos>=&yypParser->yystack[yypParser->yystksz-1] ){
      if( yyGrowStack(yypParser) ){
        yyStackOverflow(yypParser);
        return;
      }
      yymsp = yypParser->yytos;
    }
#endif
  }

  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
/********** Begin reduce actions **********************************************/
        YYMINORTYPE yylhsminor;
      case 0: /* ecmd ::= explain cmdx SEMI */
#line 107 "parse.y"
{ sqlite3FinishCoding(pParse); }
#line 2197 "parse.c"
        break;
      case 1: /* ecmd ::= SEMI */
#line 108 "parse.y"
{
  sqlite3ErrorMsg(pParse, "syntax error: empty request");
}
#line 2204 "parse.c"
        break;
      case 2: /* explain ::= EXPLAIN */
#line 113 "parse.y"
{ pParse->explain = 1; }
#line 2209 "parse.c"
        break;
      case 3: /* explain ::= EXPLAIN QUERY PLAN */
#line 114 "parse.y"
{ pParse->explain = 2; }
#line 2214 "parse.c"
        break;
      case 4: /* cmd ::= BEGIN transtype trans_opt */
#line 146 "parse.y"
{sqlite3BeginTransaction(pParse, yymsp[-1].minor.yy228);}
#line 2219 "parse.c"
        break;
      case 5: /* transtype ::= */
#line 151 "parse.y"
{yymsp[1].minor.yy228 = TK_DEFERRED;}
#line 2224 "parse.c"
        break;
      case 6: /* transtype ::= DEFERRED */
#line 152 "parse.y"
{yymsp[0].minor.yy228 = yymsp[0].major; /*A-overwrites-X*/}
#line 2229 "parse.c"
        break;
      case 7: /* cmd ::= COMMIT trans_opt */
      case 8: /* cmd ::= END trans_opt */ yytestcase(yyruleno==8);
#line 153 "parse.y"
{sqlite3CommitTransaction(pParse);}
#line 2235 "parse.c"
        break;
      case 9: /* cmd ::= ROLLBACK trans_opt */
#line 155 "parse.y"
{sqlite3RollbackTransaction(pParse);}
#line 2240 "parse.c"
        break;
      case 10: /* cmd ::= SAVEPOINT nm */
#line 159 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_BEGIN, &yymsp[0].minor.yy0);
}
#line 2247 "parse.c"
        break;
      case 11: /* cmd ::= RELEASE savepoint_opt nm */
#line 162 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_RELEASE, &yymsp[0].minor.yy0);
}
#line 2254 "parse.c"
        break;
      case 12: /* cmd ::= ROLLBACK trans_opt TO savepoint_opt nm */
#line 165 "parse.y"
{
  sqlite3Savepoint(pParse, SAVEPOINT_ROLLBACK, &yymsp[0].minor.yy0);
}
#line 2261 "parse.c"
        break;
      case 13: /* create_table ::= createkw TABLE ifnotexists nm */
#line 172 "parse.y"
{
   sqlite3StartTable(pParse,&yymsp[0].minor.yy0,yymsp[-1].minor.yy228);
}
#line 2268 "parse.c"
        break;
      case 14: /* createkw ::= CREATE */
#line 175 "parse.y"
{disableLookaside(pParse);}
#line 2273 "parse.c"
        break;
      case 15: /* ifnotexists ::= */
      case 19: /* table_options ::= */ yytestcase(yyruleno==19);
      case 39: /* autoinc ::= */ yytestcase(yyruleno==39);
      case 54: /* init_deferred_pred_opt ::= */ yytestcase(yyruleno==54);
      case 64: /* defer_subclause_opt ::= */ yytestcase(yyruleno==64);
      case 73: /* ifexists ::= */ yytestcase(yyruleno==73);
      case 87: /* distinct ::= */ yytestcase(yyruleno==87);
      case 209: /* collate ::= */ yytestcase(yyruleno==209);
#line 178 "parse.y"
{yymsp[1].minor.yy228 = 0;}
#line 2285 "parse.c"
        break;
      case 16: /* ifnotexists ::= IF NOT EXISTS */
#line 179 "parse.y"
{yymsp[-2].minor.yy228 = 1;}
#line 2290 "parse.c"
        break;
      case 17: /* create_table_args ::= LP columnlist conslist_opt RP table_options */
#line 181 "parse.y"
{
  sqlite3EndTable(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0,yymsp[0].minor.yy228,0);
}
#line 2297 "parse.c"
        break;
      case 18: /* create_table_args ::= AS select */
#line 184 "parse.y"
{
  sqlite3EndTable(pParse,0,0,0,yymsp[0].minor.yy387);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy387);
}
#line 2305 "parse.c"
        break;
      case 20: /* table_options ::= WITHOUT nm */
#line 190 "parse.y"
{
  if( yymsp[0].minor.yy0.n==5 && sqlite3_strnicmp(yymsp[0].minor.yy0.z,"rowid",5)==0 ){
    yymsp[-1].minor.yy228 = TF_WithoutRowid | TF_NoVisibleRowid;
  }else{
    yymsp[-1].minor.yy228 = 0;
    sqlite3ErrorMsg(pParse, "unknown table option: %.*s", yymsp[0].minor.yy0.n, yymsp[0].minor.yy0.z);
  }
}
#line 2317 "parse.c"
        break;
      case 21: /* columnname ::= nm typetoken */
#line 200 "parse.y"
{sqlite3AddColumn(pParse,&yymsp[-1].minor.yy0,&yymsp[0].minor.yy0);}
#line 2322 "parse.c"
        break;
      case 22: /* typetoken ::= */
      case 57: /* conslist_opt ::= */ yytestcase(yyruleno==57);
      case 93: /* as ::= */ yytestcase(yyruleno==93);
#line 240 "parse.y"
{yymsp[1].minor.yy0.n = 0; yymsp[1].minor.yy0.z = 0;}
#line 2329 "parse.c"
        break;
      case 23: /* typetoken ::= typename LP signed RP */
#line 242 "parse.y"
{
  yymsp[-3].minor.yy0.n = (int)(&yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-3].minor.yy0.z);
}
#line 2336 "parse.c"
        break;
      case 24: /* typetoken ::= typename LP signed COMMA signed RP */
#line 245 "parse.y"
{
  yymsp[-5].minor.yy0.n = (int)(&yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n] - yymsp[-5].minor.yy0.z);
}
#line 2343 "parse.c"
        break;
      case 25: /* typename ::= typename ID|STRING */
#line 250 "parse.y"
{yymsp[-1].minor.yy0.n=yymsp[0].minor.yy0.n+(int)(yymsp[0].minor.yy0.z-yymsp[-1].minor.yy0.z);}
#line 2348 "parse.c"
        break;
      case 26: /* ccons ::= CONSTRAINT nm */
      case 59: /* tcons ::= CONSTRAINT nm */ yytestcase(yyruleno==59);
#line 259 "parse.y"
{pParse->constraintName = yymsp[0].minor.yy0;}
#line 2354 "parse.c"
        break;
      case 27: /* ccons ::= DEFAULT term */
      case 29: /* ccons ::= DEFAULT PLUS term */ yytestcase(yyruleno==29);
#line 260 "parse.y"
{sqlite3AddDefaultValue(pParse,&yymsp[0].minor.yy326);}
#line 2360 "parse.c"
        break;
      case 28: /* ccons ::= DEFAULT LP expr RP */
#line 261 "parse.y"
{sqlite3AddDefaultValue(pParse,&yymsp[-1].minor.yy326);}
#line 2365 "parse.c"
        break;
      case 30: /* ccons ::= DEFAULT MINUS term */
#line 263 "parse.y"
{
  ExprSpan v;
  v.pExpr = sqlite3PExpr(pParse, TK_UMINUS, yymsp[0].minor.yy326.pExpr, 0);
  v.zStart = yymsp[-1].minor.yy0.z;
  v.zEnd = yymsp[0].minor.yy326.zEnd;
  sqlite3AddDefaultValue(pParse,&v);
}
#line 2376 "parse.c"
        break;
      case 31: /* ccons ::= DEFAULT ID|INDEXED */
#line 270 "parse.y"
{
  ExprSpan v;
  spanExpr(&v, pParse, TK_STRING, yymsp[0].minor.yy0);
  sqlite3AddDefaultValue(pParse,&v);
}
#line 2385 "parse.c"
        break;
      case 32: /* ccons ::= NOT NULL onconf */
#line 280 "parse.y"
{sqlite3AddNotNull(pParse, yymsp[0].minor.yy228);}
#line 2390 "parse.c"
        break;
      case 33: /* ccons ::= PRIMARY KEY sortorder onconf autoinc */
#line 282 "parse.y"
{sqlite3AddPrimaryKey(pParse,0,yymsp[-1].minor.yy228,yymsp[0].minor.yy228,yymsp[-2].minor.yy228);}
#line 2395 "parse.c"
        break;
      case 34: /* ccons ::= UNIQUE onconf */
#line 283 "parse.y"
{sqlite3CreateIndex(pParse,0,0,0,yymsp[0].minor.yy228,0,0,0,0,
                                   SQLITE_IDXTYPE_UNIQUE);}
#line 2401 "parse.c"
        break;
      case 35: /* ccons ::= CHECK LP expr RP */
#line 285 "parse.y"
{sqlite3AddCheckConstraint(pParse,yymsp[-1].minor.yy326.pExpr);}
#line 2406 "parse.c"
        break;
      case 36: /* ccons ::= REFERENCES nm eidlist_opt refargs */
#line 287 "parse.y"
{sqlite3CreateForeignKey(pParse,0,&yymsp[-2].minor.yy0,yymsp[-1].minor.yy258,yymsp[0].minor.yy228);}
#line 2411 "parse.c"
        break;
      case 37: /* ccons ::= defer_subclause */
#line 288 "parse.y"
{sqlite3DeferForeignKey(pParse,yymsp[0].minor.yy228);}
#line 2416 "parse.c"
        break;
      case 38: /* ccons ::= COLLATE ID|STRING */
#line 289 "parse.y"
{sqlite3AddCollateType(pParse, &yymsp[0].minor.yy0);}
#line 2421 "parse.c"
        break;
      case 40: /* autoinc ::= AUTOINCR */
#line 294 "parse.y"
{yymsp[0].minor.yy228 = 1;}
#line 2426 "parse.c"
        break;
      case 41: /* refargs ::= */
#line 302 "parse.y"
{ yymsp[1].minor.yy228 = OE_None*0x0101; /* EV: R-19803-45884 */}
#line 2431 "parse.c"
        break;
      case 42: /* refargs ::= refargs refarg */
#line 303 "parse.y"
{ yymsp[-1].minor.yy228 = (yymsp[-1].minor.yy228 & ~yymsp[0].minor.yy231.mask) | yymsp[0].minor.yy231.value; }
#line 2436 "parse.c"
        break;
      case 43: /* refarg ::= MATCH nm */
#line 305 "parse.y"
{ yymsp[-1].minor.yy231.value = 0;     yymsp[-1].minor.yy231.mask = 0x000000; }
#line 2441 "parse.c"
        break;
      case 44: /* refarg ::= ON INSERT refact */
#line 306 "parse.y"
{ yymsp[-2].minor.yy231.value = 0;     yymsp[-2].minor.yy231.mask = 0x000000; }
#line 2446 "parse.c"
        break;
      case 45: /* refarg ::= ON DELETE refact */
#line 307 "parse.y"
{ yymsp[-2].minor.yy231.value = yymsp[0].minor.yy228;     yymsp[-2].minor.yy231.mask = 0x0000ff; }
#line 2451 "parse.c"
        break;
      case 46: /* refarg ::= ON UPDATE refact */
#line 308 "parse.y"
{ yymsp[-2].minor.yy231.value = yymsp[0].minor.yy228<<8;  yymsp[-2].minor.yy231.mask = 0x00ff00; }
#line 2456 "parse.c"
        break;
      case 47: /* refact ::= SET NULL */
#line 310 "parse.y"
{ yymsp[-1].minor.yy228 = OE_SetNull;  /* EV: R-33326-45252 */}
#line 2461 "parse.c"
        break;
      case 48: /* refact ::= SET DEFAULT */
#line 311 "parse.y"
{ yymsp[-1].minor.yy228 = OE_SetDflt;  /* EV: R-33326-45252 */}
#line 2466 "parse.c"
        break;
      case 49: /* refact ::= CASCADE */
#line 312 "parse.y"
{ yymsp[0].minor.yy228 = OE_Cascade;  /* EV: R-33326-45252 */}
#line 2471 "parse.c"
        break;
      case 50: /* refact ::= RESTRICT */
#line 313 "parse.y"
{ yymsp[0].minor.yy228 = OE_Restrict; /* EV: R-33326-45252 */}
#line 2476 "parse.c"
        break;
      case 51: /* refact ::= NO ACTION */
#line 314 "parse.y"
{ yymsp[-1].minor.yy228 = OE_None;     /* EV: R-33326-45252 */}
#line 2481 "parse.c"
        break;
      case 52: /* defer_subclause ::= NOT DEFERRABLE init_deferred_pred_opt */
#line 316 "parse.y"
{yymsp[-2].minor.yy228 = 0;}
#line 2486 "parse.c"
        break;
      case 53: /* defer_subclause ::= DEFERRABLE init_deferred_pred_opt */
      case 68: /* orconf ::= OR resolvetype */ yytestcase(yyruleno==68);
      case 139: /* insert_cmd ::= INSERT orconf */ yytestcase(yyruleno==139);
#line 317 "parse.y"
{yymsp[-1].minor.yy228 = yymsp[0].minor.yy228;}
#line 2493 "parse.c"
        break;
      case 55: /* init_deferred_pred_opt ::= INITIALLY DEFERRED */
      case 72: /* ifexists ::= IF EXISTS */ yytestcase(yyruleno==72);
      case 181: /* between_op ::= NOT BETWEEN */ yytestcase(yyruleno==181);
      case 184: /* in_op ::= NOT IN */ yytestcase(yyruleno==184);
      case 210: /* collate ::= COLLATE ID|STRING */ yytestcase(yyruleno==210);
#line 320 "parse.y"
{yymsp[-1].minor.yy228 = 1;}
#line 2502 "parse.c"
        break;
      case 56: /* init_deferred_pred_opt ::= INITIALLY IMMEDIATE */
#line 321 "parse.y"
{yymsp[-1].minor.yy228 = 0;}
#line 2507 "parse.c"
        break;
      case 58: /* tconscomma ::= COMMA */
#line 327 "parse.y"
{pParse->constraintName.n = 0;}
#line 2512 "parse.c"
        break;
      case 60: /* tcons ::= PRIMARY KEY LP sortlist autoinc RP onconf */
#line 331 "parse.y"
{sqlite3AddPrimaryKey(pParse,yymsp[-3].minor.yy258,yymsp[0].minor.yy228,yymsp[-2].minor.yy228,0);}
#line 2517 "parse.c"
        break;
      case 61: /* tcons ::= UNIQUE LP sortlist RP onconf */
#line 333 "parse.y"
{sqlite3CreateIndex(pParse,0,0,yymsp[-2].minor.yy258,yymsp[0].minor.yy228,0,0,0,0,
                                       SQLITE_IDXTYPE_UNIQUE);}
#line 2523 "parse.c"
        break;
      case 62: /* tcons ::= CHECK LP expr RP onconf */
#line 336 "parse.y"
{sqlite3AddCheckConstraint(pParse,yymsp[-2].minor.yy326.pExpr);}
#line 2528 "parse.c"
        break;
      case 63: /* tcons ::= FOREIGN KEY LP eidlist RP REFERENCES nm eidlist_opt refargs defer_subclause_opt */
#line 338 "parse.y"
{
    sqlite3CreateForeignKey(pParse, yymsp[-6].minor.yy258, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy258, yymsp[-1].minor.yy228);
    sqlite3DeferForeignKey(pParse, yymsp[0].minor.yy228);
}
#line 2536 "parse.c"
        break;
      case 65: /* onconf ::= */
      case 67: /* orconf ::= */ yytestcase(yyruleno==67);
#line 352 "parse.y"
{yymsp[1].minor.yy228 = OE_Default;}
#line 2542 "parse.c"
        break;
      case 66: /* onconf ::= ON CONFLICT resolvetype */
#line 353 "parse.y"
{yymsp[-2].minor.yy228 = yymsp[0].minor.yy228;}
#line 2547 "parse.c"
        break;
      case 69: /* resolvetype ::= IGNORE */
#line 357 "parse.y"
{yymsp[0].minor.yy228 = OE_Ignore;}
#line 2552 "parse.c"
        break;
      case 70: /* resolvetype ::= REPLACE */
      case 140: /* insert_cmd ::= REPLACE */ yytestcase(yyruleno==140);
#line 358 "parse.y"
{yymsp[0].minor.yy228 = OE_Replace;}
#line 2558 "parse.c"
        break;
      case 71: /* cmd ::= DROP TABLE ifexists fullname */
#line 362 "parse.y"
{
  sqlite3DropTable(pParse, yymsp[0].minor.yy3, 0, yymsp[-1].minor.yy228);
}
#line 2565 "parse.c"
        break;
      case 74: /* cmd ::= createkw VIEW ifnotexists nm eidlist_opt AS select */
#line 373 "parse.y"
{
  sqlite3CreateView(pParse, &yymsp[-6].minor.yy0, &yymsp[-3].minor.yy0, yymsp[-2].minor.yy258, yymsp[0].minor.yy387, yymsp[-4].minor.yy228);
}
#line 2572 "parse.c"
        break;
      case 75: /* cmd ::= DROP VIEW ifexists fullname */
#line 376 "parse.y"
{
  sqlite3DropTable(pParse, yymsp[0].minor.yy3, 1, yymsp[-1].minor.yy228);
}
#line 2579 "parse.c"
        break;
      case 76: /* cmd ::= select */
#line 383 "parse.y"
{
  SelectDest dest = {SRT_Output, 0, 0, 0, 0, 0};
  sqlite3Select(pParse, yymsp[0].minor.yy387, &dest);
  sqlite3SelectDelete(pParse->db, yymsp[0].minor.yy387);
}
#line 2588 "parse.c"
        break;
      case 77: /* select ::= with selectnowith */
#line 420 "parse.y"
{
  Select *p = yymsp[0].minor.yy387;
  if( p ){
    p->pWith = yymsp[-1].minor.yy211;
    parserDoubleLinkSelect(pParse, p);
  }else{
    sqlite3WithDelete(pParse->db, yymsp[-1].minor.yy211);
  }
  yymsp[-1].minor.yy387 = p; /*A-overwrites-W*/
}
#line 2602 "parse.c"
        break;
      case 78: /* selectnowith ::= selectnowith multiselect_op oneselect */
#line 433 "parse.y"
{
  Select *pRhs = yymsp[0].minor.yy387;
  Select *pLhs = yymsp[-2].minor.yy387;
  if( pRhs && pRhs->pPrior ){
    SrcList *pFrom;
    Token x;
    x.n = 0;
    parserDoubleLinkSelect(pParse, pRhs);
    pFrom = sqlite3SrcListAppendFromTerm(pParse,0,0,0,&x,pRhs,0,0);
    pRhs = sqlite3SelectNew(pParse,0,pFrom,0,0,0,0,0,0,0);
  }
  if( pRhs ){
    pRhs->op = (u8)yymsp[-1].minor.yy228;
    pRhs->pPrior = pLhs;
    if( ALWAYS(pLhs) ) pLhs->selFlags &= ~SF_MultiValue;
    pRhs->selFlags &= ~SF_MultiValue;
    if( yymsp[-1].minor.yy228!=TK_ALL ) pParse->hasCompound = 1;
  }else{
    sqlite3SelectDelete(pParse->db, pLhs);
  }
  yymsp[-2].minor.yy387 = pRhs;
}
#line 2628 "parse.c"
        break;
      case 79: /* multiselect_op ::= UNION */
      case 81: /* multiselect_op ::= EXCEPT|INTERSECT */ yytestcase(yyruleno==81);
#line 456 "parse.y"
{yymsp[0].minor.yy228 = yymsp[0].major; /*A-overwrites-OP*/}
#line 2634 "parse.c"
        break;
      case 80: /* multiselect_op ::= UNION ALL */
#line 457 "parse.y"
{yymsp[-1].minor.yy228 = TK_ALL;}
#line 2639 "parse.c"
        break;
      case 82: /* oneselect ::= SELECT distinct selcollist from where_opt groupby_opt having_opt orderby_opt limit_opt */
#line 461 "parse.y"
{
#if SELECTTRACE_ENABLED
  Token s = yymsp[-8].minor.yy0; /*A-overwrites-S*/
#endif
  yymsp[-8].minor.yy387 = sqlite3SelectNew(pParse,yymsp[-6].minor.yy258,yymsp[-5].minor.yy3,yymsp[-4].minor.yy402,yymsp[-3].minor.yy258,yymsp[-2].minor.yy402,yymsp[-1].minor.yy258,yymsp[-7].minor.yy228,yymsp[0].minor.yy196.pLimit,yymsp[0].minor.yy196.pOffset);
#if SELECTTRACE_ENABLED
  /* Populate the Select.zSelName[] string that is used to help with
  ** query planner debugging, to differentiate between multiple Select
  ** objects in a complex query.
  **
  ** If the SELECT keyword is immediately followed by a C-style comment
  ** then extract the first few alphanumeric characters from within that
  ** comment to be the zSelName value.  Otherwise, the label is #N where
  ** is an integer that is incremented with each SELECT statement seen.
  */
  if( yymsp[-8].minor.yy387!=0 ){
    const char *z = s.z+6;
    int i;
    sqlite3_snprintf(sizeof(yymsp[-8].minor.yy387->zSelName), yymsp[-8].minor.yy387->zSelName, "#%d",
                     ++pParse->nSelect);
    while( z[0]==' ' ) z++;
    if( z[0]=='/' && z[1]=='*' ){
      z += 2;
      while( z[0]==' ' ) z++;
      for(i=0; sqlite3Isalnum(z[i]); i++){}
      sqlite3_snprintf(sizeof(yymsp[-8].minor.yy387->zSelName), yymsp[-8].minor.yy387->zSelName, "%.*s", i, z);
    }
  }
#endif /* SELECTRACE_ENABLED */
}
#line 2673 "parse.c"
        break;
      case 83: /* values ::= VALUES LP nexprlist RP */
#line 495 "parse.y"
{
  yymsp[-3].minor.yy387 = sqlite3SelectNew(pParse,yymsp[-1].minor.yy258,0,0,0,0,0,SF_Values,0,0);
}
#line 2680 "parse.c"
        break;
      case 84: /* values ::= values COMMA LP exprlist RP */
#line 498 "parse.y"
{
  Select *pRight, *pLeft = yymsp[-4].minor.yy387;
  pRight = sqlite3SelectNew(pParse,yymsp[-1].minor.yy258,0,0,0,0,0,SF_Values|SF_MultiValue,0,0);
  if( ALWAYS(pLeft) ) pLeft->selFlags &= ~SF_MultiValue;
  if( pRight ){
    pRight->op = TK_ALL;
    pRight->pPrior = pLeft;
    yymsp[-4].minor.yy387 = pRight;
  }else{
    yymsp[-4].minor.yy387 = pLeft;
  }
}
#line 2696 "parse.c"
        break;
      case 85: /* distinct ::= DISTINCT */
#line 515 "parse.y"
{yymsp[0].minor.yy228 = SF_Distinct;}
#line 2701 "parse.c"
        break;
      case 86: /* distinct ::= ALL */
#line 516 "parse.y"
{yymsp[0].minor.yy228 = SF_All;}
#line 2706 "parse.c"
        break;
      case 88: /* sclp ::= */
      case 114: /* orderby_opt ::= */ yytestcase(yyruleno==114);
      case 121: /* groupby_opt ::= */ yytestcase(yyruleno==121);
      case 197: /* exprlist ::= */ yytestcase(yyruleno==197);
      case 200: /* paren_exprlist ::= */ yytestcase(yyruleno==200);
      case 205: /* eidlist_opt ::= */ yytestcase(yyruleno==205);
#line 529 "parse.y"
{yymsp[1].minor.yy258 = 0;}
#line 2716 "parse.c"
        break;
      case 89: /* selcollist ::= sclp expr as */
#line 530 "parse.y"
{
   yymsp[-2].minor.yy258 = sqlite3ExprListAppend(pParse, yymsp[-2].minor.yy258, yymsp[-1].minor.yy326.pExpr);
   if( yymsp[0].minor.yy0.n>0 ) sqlite3ExprListSetName(pParse, yymsp[-2].minor.yy258, &yymsp[0].minor.yy0, 1);
   sqlite3ExprListSetSpan(pParse,yymsp[-2].minor.yy258,&yymsp[-1].minor.yy326);
}
#line 2725 "parse.c"
        break;
      case 90: /* selcollist ::= sclp STAR */
#line 535 "parse.y"
{
  Expr *p = sqlite3Expr(pParse->db, TK_ASTERISK, 0);
  yymsp[-1].minor.yy258 = sqlite3ExprListAppend(pParse, yymsp[-1].minor.yy258, p);
}
#line 2733 "parse.c"
        break;
      case 91: /* selcollist ::= sclp nm DOT STAR */
#line 539 "parse.y"
{
  Expr *pRight = sqlite3PExpr(pParse, TK_ASTERISK, 0, 0);
  Expr *pLeft = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *pDot = sqlite3PExpr(pParse, TK_DOT, pLeft, pRight);
  yymsp[-3].minor.yy258 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy258, pDot);
}
#line 2743 "parse.c"
        break;
      case 92: /* as ::= AS nm */
      case 218: /* plus_num ::= PLUS INTEGER|FLOAT */ yytestcase(yyruleno==218);
      case 219: /* minus_num ::= MINUS INTEGER|FLOAT */ yytestcase(yyruleno==219);
#line 550 "parse.y"
{yymsp[-1].minor.yy0 = yymsp[0].minor.yy0;}
#line 2750 "parse.c"
        break;
      case 94: /* from ::= */
#line 564 "parse.y"
{yymsp[1].minor.yy3 = sqlite3DbMallocZero(pParse->db, sizeof(*yymsp[1].minor.yy3));}
#line 2755 "parse.c"
        break;
      case 95: /* from ::= FROM seltablist */
#line 565 "parse.y"
{
  yymsp[-1].minor.yy3 = yymsp[0].minor.yy3;
  sqlite3SrcListShiftJoinType(yymsp[-1].minor.yy3);
}
#line 2763 "parse.c"
        break;
      case 96: /* stl_prefix ::= seltablist joinop */
#line 573 "parse.y"
{
   if( ALWAYS(yymsp[-1].minor.yy3 && yymsp[-1].minor.yy3->nSrc>0) ) yymsp[-1].minor.yy3->a[yymsp[-1].minor.yy3->nSrc-1].fg.jointype = (u8)yymsp[0].minor.yy228;
}
#line 2770 "parse.c"
        break;
      case 97: /* stl_prefix ::= */
#line 576 "parse.y"
{yymsp[1].minor.yy3 = 0;}
#line 2775 "parse.c"
        break;
      case 98: /* seltablist ::= stl_prefix nm as indexed_opt on_opt using_opt */
#line 578 "parse.y"
{
  yymsp[-5].minor.yy3 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-5].minor.yy3,&yymsp[-4].minor.yy0,0,&yymsp[-3].minor.yy0,0,yymsp[-1].minor.yy402,yymsp[0].minor.yy400);
  sqlite3SrcListIndexedBy(pParse, yymsp[-5].minor.yy3, &yymsp[-2].minor.yy0);
}
#line 2783 "parse.c"
        break;
      case 99: /* seltablist ::= stl_prefix nm LP exprlist RP as on_opt using_opt */
#line 583 "parse.y"
{
  yymsp[-7].minor.yy3 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-7].minor.yy3,&yymsp[-6].minor.yy0,0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy402,yymsp[0].minor.yy400);
  sqlite3SrcListFuncArgs(pParse, yymsp[-7].minor.yy3, yymsp[-4].minor.yy258);
}
#line 2791 "parse.c"
        break;
      case 100: /* seltablist ::= stl_prefix LP select RP as on_opt using_opt */
#line 589 "parse.y"
{
    yymsp[-6].minor.yy3 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy3,0,0,&yymsp[-2].minor.yy0,yymsp[-4].minor.yy387,yymsp[-1].minor.yy402,yymsp[0].minor.yy400);
  }
#line 2798 "parse.c"
        break;
      case 101: /* seltablist ::= stl_prefix LP seltablist RP as on_opt using_opt */
#line 593 "parse.y"
{
    if( yymsp[-6].minor.yy3==0 && yymsp[-2].minor.yy0.n==0 && yymsp[-1].minor.yy402==0 && yymsp[0].minor.yy400==0 ){
      yymsp[-6].minor.yy3 = yymsp[-4].minor.yy3;
    }else if( yymsp[-4].minor.yy3->nSrc==1 ){
      yymsp[-6].minor.yy3 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy3,0,0,&yymsp[-2].minor.yy0,0,yymsp[-1].minor.yy402,yymsp[0].minor.yy400);
      if( yymsp[-6].minor.yy3 ){
        struct SrcList_item *pNew = &yymsp[-6].minor.yy3->a[yymsp[-6].minor.yy3->nSrc-1];
        struct SrcList_item *pOld = yymsp[-4].minor.yy3->a;
        pNew->zName = pOld->zName;
        pNew->zDatabase = pOld->zDatabase;
        pNew->pSelect = pOld->pSelect;
        pOld->zName = pOld->zDatabase = 0;
        pOld->pSelect = 0;
      }
      sqlite3SrcListDelete(pParse->db, yymsp[-4].minor.yy3);
    }else{
      Select *pSubquery;
      sqlite3SrcListShiftJoinType(yymsp[-4].minor.yy3);
      pSubquery = sqlite3SelectNew(pParse,0,yymsp[-4].minor.yy3,0,0,0,0,SF_NestedFrom,0,0);
      yymsp[-6].minor.yy3 = sqlite3SrcListAppendFromTerm(pParse,yymsp[-6].minor.yy3,0,0,&yymsp[-2].minor.yy0,pSubquery,yymsp[-1].minor.yy402,yymsp[0].minor.yy400);
    }
  }
#line 2824 "parse.c"
        break;
      case 102: /* fullname ::= nm */
#line 620 "parse.y"
{yymsp[0].minor.yy3 = sqlite3SrcListAppend(pParse->db,0,&yymsp[0].minor.yy0,0); /*A-overwrites-X*/}
#line 2829 "parse.c"
        break;
      case 103: /* joinop ::= COMMA|JOIN */
#line 623 "parse.y"
{ yymsp[0].minor.yy228 = JT_INNER; }
#line 2834 "parse.c"
        break;
      case 104: /* joinop ::= JOIN_KW JOIN */
#line 625 "parse.y"
{yymsp[-1].minor.yy228 = sqlite3JoinType(pParse,&yymsp[-1].minor.yy0,0,0);  /*X-overwrites-A*/}
#line 2839 "parse.c"
        break;
      case 105: /* joinop ::= JOIN_KW nm JOIN */
#line 627 "parse.y"
{yymsp[-2].minor.yy228 = sqlite3JoinType(pParse,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0,0); /*X-overwrites-A*/}
#line 2844 "parse.c"
        break;
      case 106: /* joinop ::= JOIN_KW nm nm JOIN */
#line 629 "parse.y"
{yymsp[-3].minor.yy228 = sqlite3JoinType(pParse,&yymsp[-3].minor.yy0,&yymsp[-2].minor.yy0,&yymsp[-1].minor.yy0);/*X-overwrites-A*/}
#line 2849 "parse.c"
        break;
      case 107: /* on_opt ::= ON expr */
      case 124: /* having_opt ::= HAVING expr */ yytestcase(yyruleno==124);
      case 131: /* where_opt ::= WHERE expr */ yytestcase(yyruleno==131);
      case 193: /* case_else ::= ELSE expr */ yytestcase(yyruleno==193);
#line 633 "parse.y"
{yymsp[-1].minor.yy402 = yymsp[0].minor.yy326.pExpr;}
#line 2857 "parse.c"
        break;
      case 108: /* on_opt ::= */
      case 123: /* having_opt ::= */ yytestcase(yyruleno==123);
      case 130: /* where_opt ::= */ yytestcase(yyruleno==130);
      case 194: /* case_else ::= */ yytestcase(yyruleno==194);
      case 196: /* case_operand ::= */ yytestcase(yyruleno==196);
#line 634 "parse.y"
{yymsp[1].minor.yy402 = 0;}
#line 2866 "parse.c"
        break;
      case 109: /* indexed_opt ::= */
#line 647 "parse.y"
{yymsp[1].minor.yy0.z=0; yymsp[1].minor.yy0.n=0;}
#line 2871 "parse.c"
        break;
      case 110: /* indexed_opt ::= INDEXED BY nm */
#line 648 "parse.y"
{yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;}
#line 2876 "parse.c"
        break;
      case 111: /* indexed_opt ::= NOT INDEXED */
#line 649 "parse.y"
{yymsp[-1].minor.yy0.z=0; yymsp[-1].minor.yy0.n=1;}
#line 2881 "parse.c"
        break;
      case 112: /* using_opt ::= USING LP idlist RP */
#line 653 "parse.y"
{yymsp[-3].minor.yy400 = yymsp[-1].minor.yy400;}
#line 2886 "parse.c"
        break;
      case 113: /* using_opt ::= */
      case 141: /* idlist_opt ::= */ yytestcase(yyruleno==141);
#line 654 "parse.y"
{yymsp[1].minor.yy400 = 0;}
#line 2892 "parse.c"
        break;
      case 115: /* orderby_opt ::= ORDER BY sortlist */
      case 122: /* groupby_opt ::= GROUP BY nexprlist */ yytestcase(yyruleno==122);
#line 668 "parse.y"
{yymsp[-2].minor.yy258 = yymsp[0].minor.yy258;}
#line 2898 "parse.c"
        break;
      case 116: /* sortlist ::= sortlist COMMA expr sortorder */
#line 669 "parse.y"
{
  yymsp[-3].minor.yy258 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy258,yymsp[-1].minor.yy326.pExpr);
  sqlite3ExprListSetSortOrder(yymsp[-3].minor.yy258,yymsp[0].minor.yy228);
}
#line 2906 "parse.c"
        break;
      case 117: /* sortlist ::= expr sortorder */
#line 673 "parse.y"
{
  yymsp[-1].minor.yy258 = sqlite3ExprListAppend(pParse,0,yymsp[-1].minor.yy326.pExpr); /*A-overwrites-Y*/
  sqlite3ExprListSetSortOrder(yymsp[-1].minor.yy258,yymsp[0].minor.yy228);
}
#line 2914 "parse.c"
        break;
      case 118: /* sortorder ::= ASC */
#line 680 "parse.y"
{yymsp[0].minor.yy228 = SQLITE_SO_ASC;}
#line 2919 "parse.c"
        break;
      case 119: /* sortorder ::= DESC */
#line 681 "parse.y"
{yymsp[0].minor.yy228 = SQLITE_SO_DESC;}
#line 2924 "parse.c"
        break;
      case 120: /* sortorder ::= */
#line 682 "parse.y"
{yymsp[1].minor.yy228 = SQLITE_SO_UNDEFINED;}
#line 2929 "parse.c"
        break;
      case 125: /* limit_opt ::= */
#line 707 "parse.y"
{yymsp[1].minor.yy196.pLimit = 0; yymsp[1].minor.yy196.pOffset = 0;}
#line 2934 "parse.c"
        break;
      case 126: /* limit_opt ::= LIMIT expr */
#line 708 "parse.y"
{yymsp[-1].minor.yy196.pLimit = yymsp[0].minor.yy326.pExpr; yymsp[-1].minor.yy196.pOffset = 0;}
#line 2939 "parse.c"
        break;
      case 127: /* limit_opt ::= LIMIT expr OFFSET expr */
#line 710 "parse.y"
{yymsp[-3].minor.yy196.pLimit = yymsp[-2].minor.yy326.pExpr; yymsp[-3].minor.yy196.pOffset = yymsp[0].minor.yy326.pExpr;}
#line 2944 "parse.c"
        break;
      case 128: /* limit_opt ::= LIMIT expr COMMA expr */
#line 712 "parse.y"
{yymsp[-3].minor.yy196.pOffset = yymsp[-2].minor.yy326.pExpr; yymsp[-3].minor.yy196.pLimit = yymsp[0].minor.yy326.pExpr;}
#line 2949 "parse.c"
        break;
      case 129: /* cmd ::= with DELETE FROM fullname indexed_opt where_opt */
#line 729 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-5].minor.yy211, 1);
  sqlite3SrcListIndexedBy(pParse, yymsp[-2].minor.yy3, &yymsp[-1].minor.yy0);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3DeleteFrom(pParse,yymsp[-2].minor.yy3,yymsp[0].minor.yy402);
}
#line 2961 "parse.c"
        break;
      case 132: /* cmd ::= with UPDATE orconf fullname indexed_opt SET setlist where_opt */
#line 762 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-7].minor.yy211, 1);
  sqlite3SrcListIndexedBy(pParse, yymsp[-4].minor.yy3, &yymsp[-3].minor.yy0);
  sqlite3ExprListCheckLength(pParse,yymsp[-1].minor.yy258,"set list"); 
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Update(pParse,yymsp[-4].minor.yy3,yymsp[-1].minor.yy258,yymsp[0].minor.yy402,yymsp[-5].minor.yy228);
}
#line 2974 "parse.c"
        break;
      case 133: /* setlist ::= setlist COMMA nm EQ expr */
#line 776 "parse.y"
{
  yymsp[-4].minor.yy258 = sqlite3ExprListAppend(pParse, yymsp[-4].minor.yy258, yymsp[0].minor.yy326.pExpr);
  sqlite3ExprListSetName(pParse, yymsp[-4].minor.yy258, &yymsp[-2].minor.yy0, 1);
}
#line 2982 "parse.c"
        break;
      case 134: /* setlist ::= setlist COMMA LP idlist RP EQ expr */
#line 780 "parse.y"
{
  yymsp[-6].minor.yy258 = sqlite3ExprListAppendVector(pParse, yymsp[-6].minor.yy258, yymsp[-3].minor.yy400, yymsp[0].minor.yy326.pExpr);
}
#line 2989 "parse.c"
        break;
      case 135: /* setlist ::= nm EQ expr */
#line 783 "parse.y"
{
  yylhsminor.yy258 = sqlite3ExprListAppend(pParse, 0, yymsp[0].minor.yy326.pExpr);
  sqlite3ExprListSetName(pParse, yylhsminor.yy258, &yymsp[-2].minor.yy0, 1);
}
#line 2997 "parse.c"
  yymsp[-2].minor.yy258 = yylhsminor.yy258;
        break;
      case 136: /* setlist ::= LP idlist RP EQ expr */
#line 787 "parse.y"
{
  yymsp[-4].minor.yy258 = sqlite3ExprListAppendVector(pParse, 0, yymsp[-3].minor.yy400, yymsp[0].minor.yy326.pExpr);
}
#line 3005 "parse.c"
        break;
      case 137: /* cmd ::= with insert_cmd INTO fullname idlist_opt select */
#line 793 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-5].minor.yy211, 1);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Insert(pParse, yymsp[-2].minor.yy3, yymsp[0].minor.yy387, yymsp[-1].minor.yy400, yymsp[-4].minor.yy228);
}
#line 3016 "parse.c"
        break;
      case 138: /* cmd ::= with insert_cmd INTO fullname idlist_opt DEFAULT VALUES */
#line 801 "parse.y"
{
  sqlite3WithPush(pParse, yymsp[-6].minor.yy211, 1);
  sqlSubProgramsRemaining = SQL_MAX_COMPILING_TRIGGERS;
  /* Instruct SQL to initate Tarantool's transaction.  */
  pParse->initiateTTrans = true;
  sqlite3Insert(pParse, yymsp[-3].minor.yy3, 0, yymsp[-2].minor.yy400, yymsp[-5].minor.yy228);
}
#line 3027 "parse.c"
        break;
      case 142: /* idlist_opt ::= LP idlist RP */
#line 819 "parse.y"
{yymsp[-2].minor.yy400 = yymsp[-1].minor.yy400;}
#line 3032 "parse.c"
        break;
      case 143: /* idlist ::= idlist COMMA nm */
#line 821 "parse.y"
{yymsp[-2].minor.yy400 = sqlite3IdListAppend(pParse->db,yymsp[-2].minor.yy400,&yymsp[0].minor.yy0);}
#line 3037 "parse.c"
        break;
      case 144: /* idlist ::= nm */
#line 823 "parse.y"
{yymsp[0].minor.yy400 = sqlite3IdListAppend(pParse->db,0,&yymsp[0].minor.yy0); /*A-overwrites-Y*/}
#line 3042 "parse.c"
        break;
      case 145: /* expr ::= LP expr RP */
#line 873 "parse.y"
{spanSet(&yymsp[-2].minor.yy326,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/  yymsp[-2].minor.yy326.pExpr = yymsp[-1].minor.yy326.pExpr;}
#line 3047 "parse.c"
        break;
      case 146: /* term ::= NULL */
      case 150: /* term ::= FLOAT|BLOB */ yytestcase(yyruleno==150);
      case 151: /* term ::= STRING */ yytestcase(yyruleno==151);
#line 874 "parse.y"
{spanExpr(&yymsp[0].minor.yy326,pParse,yymsp[0].major,yymsp[0].minor.yy0);/*A-overwrites-X*/}
#line 3054 "parse.c"
        break;
      case 147: /* expr ::= ID|INDEXED */
      case 148: /* expr ::= JOIN_KW */ yytestcase(yyruleno==148);
#line 875 "parse.y"
{spanExpr(&yymsp[0].minor.yy326,pParse,TK_ID,yymsp[0].minor.yy0); /*A-overwrites-X*/}
#line 3060 "parse.c"
        break;
      case 149: /* expr ::= nm DOT nm */
#line 877 "parse.y"
{
  Expr *temp1 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[-2].minor.yy0, 1);
  Expr *temp2 = sqlite3ExprAlloc(pParse->db, TK_ID, &yymsp[0].minor.yy0, 1);
  spanSet(&yymsp[-2].minor.yy326,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-2].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_DOT, temp1, temp2);
}
#line 3070 "parse.c"
        break;
      case 152: /* term ::= INTEGER */
#line 885 "parse.y"
{
  yylhsminor.yy326.pExpr = sqlite3ExprAlloc(pParse->db, TK_INTEGER, &yymsp[0].minor.yy0, 1);
  yylhsminor.yy326.zStart = yymsp[0].minor.yy0.z;
  yylhsminor.yy326.zEnd = yymsp[0].minor.yy0.z + yymsp[0].minor.yy0.n;
  if( yylhsminor.yy326.pExpr ) yylhsminor.yy326.pExpr->flags |= EP_Leaf;
}
#line 3080 "parse.c"
  yymsp[0].minor.yy326 = yylhsminor.yy326;
        break;
      case 153: /* expr ::= VARIABLE */
#line 891 "parse.y"
{
  if( !(yymsp[0].minor.yy0.z[0]=='#' && sqlite3Isdigit(yymsp[0].minor.yy0.z[1])) ){
    u32 n = yymsp[0].minor.yy0.n;
    spanExpr(&yymsp[0].minor.yy326, pParse, TK_VARIABLE, yymsp[0].minor.yy0);
    sqlite3ExprAssignVarNumber(pParse, yymsp[0].minor.yy326.pExpr, n);
  }else{
    /* When doing a nested parse, one can include terms in an expression
    ** that look like this:   #1 #2 ...  These terms refer to registers
    ** in the virtual machine.  #N is the N-th register. */
    Token t = yymsp[0].minor.yy0; /*A-overwrites-X*/
    assert( t.n>=2 );
    spanSet(&yymsp[0].minor.yy326, &t, &t);
    if( pParse->nested==0 ){
      sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &t);
      yymsp[0].minor.yy326.pExpr = 0;
    }else{
      yymsp[0].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_REGISTER, 0, 0);
      if( yymsp[0].minor.yy326.pExpr ) sqlite3GetInt32(&t.z[1], &yymsp[0].minor.yy326.pExpr->iTable);
    }
  }
}
#line 3106 "parse.c"
        break;
      case 154: /* expr ::= expr COLLATE ID|STRING */
#line 912 "parse.y"
{
  yymsp[-2].minor.yy326.pExpr = sqlite3ExprAddCollateToken(pParse, yymsp[-2].minor.yy326.pExpr, &yymsp[0].minor.yy0, 1);
  yymsp[-2].minor.yy326.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
}
#line 3114 "parse.c"
        break;
      case 155: /* expr ::= CAST LP expr AS typetoken RP */
#line 917 "parse.y"
{
  spanSet(&yymsp[-5].minor.yy326,&yymsp[-5].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-X*/
  yymsp[-5].minor.yy326.pExpr = sqlite3ExprAlloc(pParse->db, TK_CAST, &yymsp[-1].minor.yy0, 1);
  sqlite3ExprAttachSubtrees(pParse->db, yymsp[-5].minor.yy326.pExpr, yymsp[-3].minor.yy326.pExpr, 0);
}
#line 3123 "parse.c"
        break;
      case 156: /* expr ::= ID|INDEXED LP distinct exprlist RP */
#line 923 "parse.y"
{
  if( yymsp[-1].minor.yy258 && yymsp[-1].minor.yy258->nExpr>pParse->db->aLimit[SQLITE_LIMIT_FUNCTION_ARG] ){
    sqlite3ErrorMsg(pParse, "too many arguments on function %T", &yymsp[-4].minor.yy0);
  }
  yylhsminor.yy326.pExpr = sqlite3ExprFunction(pParse, yymsp[-1].minor.yy258, &yymsp[-4].minor.yy0);
  spanSet(&yylhsminor.yy326,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0);
  if( yymsp[-2].minor.yy228==SF_Distinct && yylhsminor.yy326.pExpr ){
    yylhsminor.yy326.pExpr->flags |= EP_Distinct;
  }
}
#line 3137 "parse.c"
  yymsp[-4].minor.yy326 = yylhsminor.yy326;
        break;
      case 157: /* expr ::= ID|INDEXED LP STAR RP */
#line 933 "parse.y"
{
  yylhsminor.yy326.pExpr = sqlite3ExprFunction(pParse, 0, &yymsp[-3].minor.yy0);
  spanSet(&yylhsminor.yy326,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);
}
#line 3146 "parse.c"
  yymsp[-3].minor.yy326 = yylhsminor.yy326;
        break;
      case 158: /* term ::= CTIME_KW */
#line 937 "parse.y"
{
  yylhsminor.yy326.pExpr = sqlite3ExprFunction(pParse, 0, &yymsp[0].minor.yy0);
  spanSet(&yylhsminor.yy326, &yymsp[0].minor.yy0, &yymsp[0].minor.yy0);
}
#line 3155 "parse.c"
  yymsp[0].minor.yy326 = yylhsminor.yy326;
        break;
      case 159: /* expr ::= LP nexprlist COMMA expr RP */
#line 966 "parse.y"
{
  ExprList *pList = sqlite3ExprListAppend(pParse, yymsp[-3].minor.yy258, yymsp[-1].minor.yy326.pExpr);
  yylhsminor.yy326.pExpr = sqlite3PExpr(pParse, TK_VECTOR, 0, 0);
  if( yylhsminor.yy326.pExpr ){
    yylhsminor.yy326.pExpr->x.pList = pList;
    spanSet(&yylhsminor.yy326, &yymsp[-4].minor.yy0, &yymsp[0].minor.yy0);
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  }
}
#line 3170 "parse.c"
  yymsp[-4].minor.yy326 = yylhsminor.yy326;
        break;
      case 160: /* expr ::= expr AND expr */
      case 161: /* expr ::= expr OR expr */ yytestcase(yyruleno==161);
      case 162: /* expr ::= expr LT|GT|GE|LE expr */ yytestcase(yyruleno==162);
      case 163: /* expr ::= expr EQ|NE expr */ yytestcase(yyruleno==163);
      case 164: /* expr ::= expr BITAND|BITOR|LSHIFT|RSHIFT expr */ yytestcase(yyruleno==164);
      case 165: /* expr ::= expr PLUS|MINUS expr */ yytestcase(yyruleno==165);
      case 166: /* expr ::= expr STAR|SLASH|REM expr */ yytestcase(yyruleno==166);
      case 167: /* expr ::= expr CONCAT expr */ yytestcase(yyruleno==167);
#line 977 "parse.y"
{spanBinaryExpr(pParse,yymsp[-1].major,&yymsp[-2].minor.yy326,&yymsp[0].minor.yy326);}
#line 3183 "parse.c"
        break;
      case 168: /* likeop ::= LIKE_KW|MATCH */
#line 990 "parse.y"
{yymsp[0].minor.yy0=yymsp[0].minor.yy0;/*A-overwrites-X*/}
#line 3188 "parse.c"
        break;
      case 169: /* likeop ::= NOT LIKE_KW|MATCH */
#line 991 "parse.y"
{yymsp[-1].minor.yy0=yymsp[0].minor.yy0; yymsp[-1].minor.yy0.n|=0x80000000; /*yymsp[-1].minor.yy0-overwrite-yymsp[0].minor.yy0*/}
#line 3193 "parse.c"
        break;
      case 170: /* expr ::= expr likeop expr */
#line 992 "parse.y"
{
  ExprList *pList;
  int bNot = yymsp[-1].minor.yy0.n & 0x80000000;
  yymsp[-1].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[0].minor.yy326.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-2].minor.yy326.pExpr);
  yymsp[-2].minor.yy326.pExpr = sqlite3ExprFunction(pParse, pList, &yymsp[-1].minor.yy0);
  exprNot(pParse, bNot, &yymsp[-2].minor.yy326);
  yymsp[-2].minor.yy326.zEnd = yymsp[0].minor.yy326.zEnd;
  if( yymsp[-2].minor.yy326.pExpr ) yymsp[-2].minor.yy326.pExpr->flags |= EP_InfixFunc;
}
#line 3208 "parse.c"
        break;
      case 171: /* expr ::= expr likeop expr ESCAPE expr */
#line 1003 "parse.y"
{
  ExprList *pList;
  int bNot = yymsp[-3].minor.yy0.n & 0x80000000;
  yymsp[-3].minor.yy0.n &= 0x7fffffff;
  pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy326.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[-4].minor.yy326.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy326.pExpr);
  yymsp[-4].minor.yy326.pExpr = sqlite3ExprFunction(pParse, pList, &yymsp[-3].minor.yy0);
  exprNot(pParse, bNot, &yymsp[-4].minor.yy326);
  yymsp[-4].minor.yy326.zEnd = yymsp[0].minor.yy326.zEnd;
  if( yymsp[-4].minor.yy326.pExpr ) yymsp[-4].minor.yy326.pExpr->flags |= EP_InfixFunc;
}
#line 3224 "parse.c"
        break;
      case 172: /* expr ::= expr ISNULL|NOTNULL */
#line 1030 "parse.y"
{spanUnaryPostfix(pParse,yymsp[0].major,&yymsp[-1].minor.yy326,&yymsp[0].minor.yy0);}
#line 3229 "parse.c"
        break;
      case 173: /* expr ::= expr NOT NULL */
#line 1031 "parse.y"
{spanUnaryPostfix(pParse,TK_NOTNULL,&yymsp[-2].minor.yy326,&yymsp[0].minor.yy0);}
#line 3234 "parse.c"
        break;
      case 174: /* expr ::= expr IS expr */
#line 1052 "parse.y"
{
  spanBinaryExpr(pParse,TK_IS,&yymsp[-2].minor.yy326,&yymsp[0].minor.yy326);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy326.pExpr, yymsp[-2].minor.yy326.pExpr, TK_ISNULL);
}
#line 3242 "parse.c"
        break;
      case 175: /* expr ::= expr IS NOT expr */
#line 1056 "parse.y"
{
  spanBinaryExpr(pParse,TK_ISNOT,&yymsp[-3].minor.yy326,&yymsp[0].minor.yy326);
  binaryToUnaryIfNull(pParse, yymsp[0].minor.yy326.pExpr, yymsp[-3].minor.yy326.pExpr, TK_NOTNULL);
}
#line 3250 "parse.c"
        break;
      case 176: /* expr ::= NOT expr */
      case 177: /* expr ::= BITNOT expr */ yytestcase(yyruleno==177);
#line 1080 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy326,pParse,yymsp[-1].major,&yymsp[0].minor.yy326,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3256 "parse.c"
        break;
      case 178: /* expr ::= MINUS expr */
#line 1084 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy326,pParse,TK_UMINUS,&yymsp[0].minor.yy326,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3261 "parse.c"
        break;
      case 179: /* expr ::= PLUS expr */
#line 1086 "parse.y"
{spanUnaryPrefix(&yymsp[-1].minor.yy326,pParse,TK_UPLUS,&yymsp[0].minor.yy326,&yymsp[-1].minor.yy0);/*A-overwrites-B*/}
#line 3266 "parse.c"
        break;
      case 180: /* between_op ::= BETWEEN */
      case 183: /* in_op ::= IN */ yytestcase(yyruleno==183);
#line 1089 "parse.y"
{yymsp[0].minor.yy228 = 0;}
#line 3272 "parse.c"
        break;
      case 182: /* expr ::= expr between_op expr AND expr */
#line 1091 "parse.y"
{
  ExprList *pList = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy326.pExpr);
  pList = sqlite3ExprListAppend(pParse,pList, yymsp[0].minor.yy326.pExpr);
  yymsp[-4].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_BETWEEN, yymsp[-4].minor.yy326.pExpr, 0);
  if( yymsp[-4].minor.yy326.pExpr ){
    yymsp[-4].minor.yy326.pExpr->x.pList = pList;
  }else{
    sqlite3ExprListDelete(pParse->db, pList);
  } 
  exprNot(pParse, yymsp[-3].minor.yy228, &yymsp[-4].minor.yy326);
  yymsp[-4].minor.yy326.zEnd = yymsp[0].minor.yy326.zEnd;
}
#line 3288 "parse.c"
        break;
      case 185: /* expr ::= expr in_op LP exprlist RP */
#line 1107 "parse.y"
{
    if( yymsp[-1].minor.yy258==0 ){
      /* Expressions of the form
      **
      **      expr1 IN ()
      **      expr1 NOT IN ()
      **
      ** simplify to constants 0 (false) and 1 (true), respectively,
      ** regardless of the value of expr1.
      */
      sqlite3ExprDelete(pParse->db, yymsp[-4].minor.yy326.pExpr);
      yymsp[-4].minor.yy326.pExpr = sqlite3ExprAlloc(pParse->db, TK_INTEGER,&sqlite3IntTokens[yymsp[-3].minor.yy228],1);
    }else if( yymsp[-1].minor.yy258->nExpr==1 ){
      /* Expressions of the form:
      **
      **      expr1 IN (?1)
      **      expr1 NOT IN (?2)
      **
      ** with exactly one value on the RHS can be simplified to something
      ** like this:
      **
      **      expr1 == ?1
      **      expr1 <> ?2
      **
      ** But, the RHS of the == or <> is marked with the EP_Generic flag
      ** so that it may not contribute to the computation of comparison
      ** affinity or the collating sequence to use for comparison.  Otherwise,
      ** the semantics would be subtly different from IN or NOT IN.
      */
      Expr *pRHS = yymsp[-1].minor.yy258->a[0].pExpr;
      yymsp[-1].minor.yy258->a[0].pExpr = 0;
      sqlite3ExprListDelete(pParse->db, yymsp[-1].minor.yy258);
      /* pRHS cannot be NULL because a malloc error would have been detected
      ** before now and control would have never reached this point */
      if( ALWAYS(pRHS) ){
        pRHS->flags &= ~EP_Collate;
        pRHS->flags |= EP_Generic;
      }
      yymsp[-4].minor.yy326.pExpr = sqlite3PExpr(pParse, yymsp[-3].minor.yy228 ? TK_NE : TK_EQ, yymsp[-4].minor.yy326.pExpr, pRHS);
    }else{
      yymsp[-4].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy326.pExpr, 0);
      if( yymsp[-4].minor.yy326.pExpr ){
        yymsp[-4].minor.yy326.pExpr->x.pList = yymsp[-1].minor.yy258;
        sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy326.pExpr);
      }else{
        sqlite3ExprListDelete(pParse->db, yymsp[-1].minor.yy258);
      }
      exprNot(pParse, yymsp[-3].minor.yy228, &yymsp[-4].minor.yy326);
    }
    yymsp[-4].minor.yy326.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
  }
#line 3343 "parse.c"
        break;
      case 186: /* expr ::= LP select RP */
#line 1158 "parse.y"
{
    spanSet(&yymsp[-2].minor.yy326,&yymsp[-2].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/
    yymsp[-2].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_SELECT, 0, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-2].minor.yy326.pExpr, yymsp[-1].minor.yy387);
  }
#line 3352 "parse.c"
        break;
      case 187: /* expr ::= expr in_op LP select RP */
#line 1163 "parse.y"
{
    yymsp[-4].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-4].minor.yy326.pExpr, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-4].minor.yy326.pExpr, yymsp[-1].minor.yy387);
    exprNot(pParse, yymsp[-3].minor.yy228, &yymsp[-4].minor.yy326);
    yymsp[-4].minor.yy326.zEnd = &yymsp[0].minor.yy0.z[yymsp[0].minor.yy0.n];
  }
#line 3362 "parse.c"
        break;
      case 188: /* expr ::= expr in_op nm paren_exprlist */
#line 1169 "parse.y"
{
    SrcList *pSrc = sqlite3SrcListAppend(pParse->db, 0,&yymsp[-1].minor.yy0,0);
    Select *pSelect = sqlite3SelectNew(pParse, 0,pSrc,0,0,0,0,0,0,0);
    if( yymsp[0].minor.yy258 )  sqlite3SrcListFuncArgs(pParse, pSelect ? pSrc : 0, yymsp[0].minor.yy258);
    yymsp[-3].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_IN, yymsp[-3].minor.yy326.pExpr, 0);
    sqlite3PExprAddSelect(pParse, yymsp[-3].minor.yy326.pExpr, pSelect);
    exprNot(pParse, yymsp[-2].minor.yy228, &yymsp[-3].minor.yy326);
    yymsp[-3].minor.yy326.zEnd = &yymsp[-1].minor.yy0.z[yymsp[-1].minor.yy0.n];
  }
#line 3375 "parse.c"
        break;
      case 189: /* expr ::= EXISTS LP select RP */
#line 1178 "parse.y"
{
    Expr *p;
    spanSet(&yymsp[-3].minor.yy326,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0); /*A-overwrites-B*/
    p = yymsp[-3].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_EXISTS, 0, 0);
    sqlite3PExprAddSelect(pParse, p, yymsp[-1].minor.yy387);
  }
#line 3385 "parse.c"
        break;
      case 190: /* expr ::= CASE case_operand case_exprlist case_else END */
#line 1187 "parse.y"
{
  spanSet(&yymsp[-4].minor.yy326,&yymsp[-4].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-C*/
  yymsp[-4].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_CASE, yymsp[-3].minor.yy402, 0);
  if( yymsp[-4].minor.yy326.pExpr ){
    yymsp[-4].minor.yy326.pExpr->x.pList = yymsp[-1].minor.yy402 ? sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy258,yymsp[-1].minor.yy402) : yymsp[-2].minor.yy258;
    sqlite3ExprSetHeightAndFlags(pParse, yymsp[-4].minor.yy326.pExpr);
  }else{
    sqlite3ExprListDelete(pParse->db, yymsp[-2].minor.yy258);
    sqlite3ExprDelete(pParse->db, yymsp[-1].minor.yy402);
  }
}
#line 3400 "parse.c"
        break;
      case 191: /* case_exprlist ::= case_exprlist WHEN expr THEN expr */
#line 1200 "parse.y"
{
  yymsp[-4].minor.yy258 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy258, yymsp[-2].minor.yy326.pExpr);
  yymsp[-4].minor.yy258 = sqlite3ExprListAppend(pParse,yymsp[-4].minor.yy258, yymsp[0].minor.yy326.pExpr);
}
#line 3408 "parse.c"
        break;
      case 192: /* case_exprlist ::= WHEN expr THEN expr */
#line 1204 "parse.y"
{
  yymsp[-3].minor.yy258 = sqlite3ExprListAppend(pParse,0, yymsp[-2].minor.yy326.pExpr);
  yymsp[-3].minor.yy258 = sqlite3ExprListAppend(pParse,yymsp[-3].minor.yy258, yymsp[0].minor.yy326.pExpr);
}
#line 3416 "parse.c"
        break;
      case 195: /* case_operand ::= expr */
#line 1214 "parse.y"
{yymsp[0].minor.yy402 = yymsp[0].minor.yy326.pExpr; /*A-overwrites-X*/}
#line 3421 "parse.c"
        break;
      case 198: /* nexprlist ::= nexprlist COMMA expr */
#line 1225 "parse.y"
{yymsp[-2].minor.yy258 = sqlite3ExprListAppend(pParse,yymsp[-2].minor.yy258,yymsp[0].minor.yy326.pExpr);}
#line 3426 "parse.c"
        break;
      case 199: /* nexprlist ::= expr */
#line 1227 "parse.y"
{yymsp[0].minor.yy258 = sqlite3ExprListAppend(pParse,0,yymsp[0].minor.yy326.pExpr); /*A-overwrites-Y*/}
#line 3431 "parse.c"
        break;
      case 201: /* paren_exprlist ::= LP exprlist RP */
      case 206: /* eidlist_opt ::= LP eidlist RP */ yytestcase(yyruleno==206);
#line 1235 "parse.y"
{yymsp[-2].minor.yy258 = yymsp[-1].minor.yy258;}
#line 3437 "parse.c"
        break;
      case 202: /* cmd ::= createkw uniqueflag INDEX ifnotexists nm ON nm LP sortlist RP where_opt */
#line 1242 "parse.y"
{
  sqlite3CreateIndex(pParse, &yymsp[-6].minor.yy0, 
                     sqlite3SrcListAppend(pParse->db,0,&yymsp[-4].minor.yy0,0), yymsp[-2].minor.yy258, yymsp[-9].minor.yy228,
                      &yymsp[-10].minor.yy0, yymsp[0].minor.yy402, SQLITE_SO_ASC, yymsp[-7].minor.yy228, SQLITE_IDXTYPE_APPDEF);
}
#line 3446 "parse.c"
        break;
      case 203: /* uniqueflag ::= UNIQUE */
      case 243: /* raisetype ::= ABORT */ yytestcase(yyruleno==243);
#line 1249 "parse.y"
{yymsp[0].minor.yy228 = OE_Abort;}
#line 3452 "parse.c"
        break;
      case 204: /* uniqueflag ::= */
#line 1250 "parse.y"
{yymsp[1].minor.yy228 = OE_None;}
#line 3457 "parse.c"
        break;
      case 207: /* eidlist ::= eidlist COMMA nm collate sortorder */
#line 1293 "parse.y"
{
  yymsp[-4].minor.yy258 = parserAddExprIdListTerm(pParse, yymsp[-4].minor.yy258, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy228, yymsp[0].minor.yy228);
}
#line 3464 "parse.c"
        break;
      case 208: /* eidlist ::= nm collate sortorder */
#line 1296 "parse.y"
{
  yymsp[-2].minor.yy258 = parserAddExprIdListTerm(pParse, 0, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy228, yymsp[0].minor.yy228); /*A-overwrites-Y*/
}
#line 3471 "parse.c"
        break;
      case 211: /* cmd ::= DROP INDEX ifexists fullname ON nm */
#line 1307 "parse.y"
{
    sqlite3DropIndex(pParse, yymsp[-2].minor.yy3, &yymsp[0].minor.yy0, yymsp[-3].minor.yy228);
}
#line 3478 "parse.c"
        break;
      case 212: /* cmd ::= PRAGMA nm */
#line 1314 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[0].minor.yy0,0,0,0,0);
}
#line 3485 "parse.c"
        break;
      case 213: /* cmd ::= PRAGMA nm EQ nmnum */
#line 1317 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-2].minor.yy0,0,&yymsp[0].minor.yy0,0,0);
}
#line 3492 "parse.c"
        break;
      case 214: /* cmd ::= PRAGMA nm LP nmnum RP */
#line 1320 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,0,&yymsp[-1].minor.yy0,0,0);
}
#line 3499 "parse.c"
        break;
      case 215: /* cmd ::= PRAGMA nm EQ minus_num */
#line 1323 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-2].minor.yy0,0,&yymsp[0].minor.yy0,0,1);
}
#line 3506 "parse.c"
        break;
      case 216: /* cmd ::= PRAGMA nm LP minus_num RP */
#line 1326 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-3].minor.yy0,0,&yymsp[-1].minor.yy0,0,1);
}
#line 3513 "parse.c"
        break;
      case 217: /* cmd ::= PRAGMA nm EQ nm DOT nm */
#line 1329 "parse.y"
{
    sqlite3Pragma(pParse,&yymsp[-4].minor.yy0,0,&yymsp[0].minor.yy0,&yymsp[-2].minor.yy0,0);
}
#line 3520 "parse.c"
        break;
      case 220: /* cmd ::= createkw trigger_decl BEGIN trigger_cmd_list END */
#line 1349 "parse.y"
{
  Token all;
  all.z = yymsp[-3].minor.yy0.z;
  all.n = (int)(yymsp[0].minor.yy0.z - yymsp[-3].minor.yy0.z) + yymsp[0].minor.yy0.n;
  sqlite3FinishTrigger(pParse, yymsp[-1].minor.yy19, &all);
}
#line 3530 "parse.c"
        break;
      case 221: /* trigger_decl ::= TRIGGER ifnotexists nm trigger_time trigger_event ON fullname foreach_clause when_clause */
#line 1358 "parse.y"
{
  sqlite3BeginTrigger(pParse, &yymsp[-6].minor.yy0, yymsp[-5].minor.yy228, yymsp[-4].minor.yy466.a, yymsp[-4].minor.yy466.b, yymsp[-2].minor.yy3, yymsp[0].minor.yy402, yymsp[-7].minor.yy228);
  yymsp[-8].minor.yy0 = yymsp[-6].minor.yy0; /*yymsp[-8].minor.yy0-overwrites-T*/
}
#line 3538 "parse.c"
        break;
      case 222: /* trigger_time ::= BEFORE */
#line 1364 "parse.y"
{ yymsp[0].minor.yy228 = TK_BEFORE; }
#line 3543 "parse.c"
        break;
      case 223: /* trigger_time ::= AFTER */
#line 1365 "parse.y"
{ yymsp[0].minor.yy228 = TK_AFTER;  }
#line 3548 "parse.c"
        break;
      case 224: /* trigger_time ::= INSTEAD OF */
#line 1366 "parse.y"
{ yymsp[-1].minor.yy228 = TK_INSTEAD;}
#line 3553 "parse.c"
        break;
      case 225: /* trigger_time ::= */
#line 1367 "parse.y"
{ yymsp[1].minor.yy228 = TK_BEFORE; }
#line 3558 "parse.c"
        break;
      case 226: /* trigger_event ::= DELETE|INSERT */
      case 227: /* trigger_event ::= UPDATE */ yytestcase(yyruleno==227);
#line 1371 "parse.y"
{yymsp[0].minor.yy466.a = yymsp[0].major; /*A-overwrites-X*/ yymsp[0].minor.yy466.b = 0;}
#line 3564 "parse.c"
        break;
      case 228: /* trigger_event ::= UPDATE OF idlist */
#line 1373 "parse.y"
{yymsp[-2].minor.yy466.a = TK_UPDATE; yymsp[-2].minor.yy466.b = yymsp[0].minor.yy400;}
#line 3569 "parse.c"
        break;
      case 229: /* when_clause ::= */
#line 1380 "parse.y"
{ yymsp[1].minor.yy402 = 0; }
#line 3574 "parse.c"
        break;
      case 230: /* when_clause ::= WHEN expr */
#line 1381 "parse.y"
{ yymsp[-1].minor.yy402 = yymsp[0].minor.yy326.pExpr; }
#line 3579 "parse.c"
        break;
      case 231: /* trigger_cmd_list ::= trigger_cmd_list trigger_cmd SEMI */
#line 1385 "parse.y"
{
  assert( yymsp[-2].minor.yy19!=0 );
  yymsp[-2].minor.yy19->pLast->pNext = yymsp[-1].minor.yy19;
  yymsp[-2].minor.yy19->pLast = yymsp[-1].minor.yy19;
}
#line 3588 "parse.c"
        break;
      case 232: /* trigger_cmd_list ::= trigger_cmd SEMI */
#line 1390 "parse.y"
{ 
  assert( yymsp[-1].minor.yy19!=0 );
  yymsp[-1].minor.yy19->pLast = yymsp[-1].minor.yy19;
}
#line 3596 "parse.c"
        break;
      case 233: /* trnm ::= nm DOT nm */
#line 1401 "parse.y"
{
  yymsp[-2].minor.yy0 = yymsp[0].minor.yy0;
  sqlite3ErrorMsg(pParse, 
        "qualified table names are not allowed on INSERT, UPDATE, and DELETE "
        "statements within triggers");
}
#line 3606 "parse.c"
        break;
      case 234: /* tridxby ::= INDEXED BY nm */
#line 1413 "parse.y"
{
  sqlite3ErrorMsg(pParse,
        "the INDEXED BY clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
#line 3615 "parse.c"
        break;
      case 235: /* tridxby ::= NOT INDEXED */
#line 1418 "parse.y"
{
  sqlite3ErrorMsg(pParse,
        "the NOT INDEXED clause is not allowed on UPDATE or DELETE statements "
        "within triggers");
}
#line 3624 "parse.c"
        break;
      case 236: /* trigger_cmd ::= UPDATE orconf trnm tridxby SET setlist where_opt */
#line 1431 "parse.y"
{yymsp[-6].minor.yy19 = sqlite3TriggerUpdateStep(pParse->db, &yymsp[-4].minor.yy0, yymsp[-1].minor.yy258, yymsp[0].minor.yy402, yymsp[-5].minor.yy228);}
#line 3629 "parse.c"
        break;
      case 237: /* trigger_cmd ::= insert_cmd INTO trnm idlist_opt select */
#line 1435 "parse.y"
{yymsp[-4].minor.yy19 = sqlite3TriggerInsertStep(pParse->db, &yymsp[-2].minor.yy0, yymsp[-1].minor.yy400, yymsp[0].minor.yy387, yymsp[-4].minor.yy228);/*A-overwrites-R*/}
#line 3634 "parse.c"
        break;
      case 238: /* trigger_cmd ::= DELETE FROM trnm tridxby where_opt */
#line 1439 "parse.y"
{yymsp[-4].minor.yy19 = sqlite3TriggerDeleteStep(pParse->db, &yymsp[-2].minor.yy0, yymsp[0].minor.yy402);}
#line 3639 "parse.c"
        break;
      case 239: /* trigger_cmd ::= select */
#line 1443 "parse.y"
{yymsp[0].minor.yy19 = sqlite3TriggerSelectStep(pParse->db, yymsp[0].minor.yy387); /*A-overwrites-X*/}
#line 3644 "parse.c"
        break;
      case 240: /* expr ::= RAISE LP IGNORE RP */
#line 1446 "parse.y"
{
  spanSet(&yymsp[-3].minor.yy326,&yymsp[-3].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-X*/
  yymsp[-3].minor.yy326.pExpr = sqlite3PExpr(pParse, TK_RAISE, 0, 0); 
  if( yymsp[-3].minor.yy326.pExpr ){
    yymsp[-3].minor.yy326.pExpr->affinity = OE_Ignore;
  }
}
#line 3655 "parse.c"
        break;
      case 241: /* expr ::= RAISE LP raisetype COMMA STRING RP */
#line 1453 "parse.y"
{
  spanSet(&yymsp[-5].minor.yy326,&yymsp[-5].minor.yy0,&yymsp[0].minor.yy0);  /*A-overwrites-X*/
  yymsp[-5].minor.yy326.pExpr = sqlite3ExprAlloc(pParse->db, TK_RAISE, &yymsp[-1].minor.yy0, 1); 
  if( yymsp[-5].minor.yy326.pExpr ) {
    yymsp[-5].minor.yy326.pExpr->affinity = (char)yymsp[-3].minor.yy228;
  }
}
#line 3666 "parse.c"
        break;
      case 242: /* raisetype ::= ROLLBACK */
#line 1463 "parse.y"
{yymsp[0].minor.yy228 = OE_Rollback;}
#line 3671 "parse.c"
        break;
      case 244: /* raisetype ::= FAIL */
#line 1465 "parse.y"
{yymsp[0].minor.yy228 = OE_Fail;}
#line 3676 "parse.c"
        break;
      case 245: /* cmd ::= DROP TRIGGER ifexists fullname */
#line 1470 "parse.y"
{
  sqlite3DropTrigger(pParse,yymsp[0].minor.yy3,yymsp[-1].minor.yy228);
}
#line 3683 "parse.c"
        break;
      case 246: /* cmd ::= REINDEX */
#line 1477 "parse.y"
{sqlite3Reindex(pParse, 0, 0);}
#line 3688 "parse.c"
        break;
      case 247: /* cmd ::= REINDEX nm */
#line 1478 "parse.y"
{sqlite3Reindex(pParse, &yymsp[0].minor.yy0, 0);}
#line 3693 "parse.c"
        break;
      case 248: /* cmd ::= REINDEX nm ON nm */
#line 1479 "parse.y"
{sqlite3Reindex(pParse, &yymsp[-2].minor.yy0, &yymsp[0].minor.yy0);}
#line 3698 "parse.c"
        break;
      case 249: /* cmd ::= ANALYZE */
#line 1484 "parse.y"
{sqlite3Analyze(pParse, 0);}
#line 3703 "parse.c"
        break;
      case 250: /* cmd ::= ANALYZE nm */
#line 1485 "parse.y"
{sqlite3Analyze(pParse, &yymsp[0].minor.yy0);}
#line 3708 "parse.c"
        break;
      case 251: /* cmd ::= ALTER TABLE fullname RENAME TO nm */
#line 1490 "parse.y"
{
  sqlite3AlterRenameTable(pParse,yymsp[-3].minor.yy3,&yymsp[0].minor.yy0);
}
#line 3715 "parse.c"
        break;
      case 252: /* cmd ::= ALTER TABLE add_column_fullname ADD kwcolumn_opt columnname carglist */
#line 1494 "parse.y"
{
  yymsp[-1].minor.yy0.n = (int)(pParse->sLastToken.z-yymsp[-1].minor.yy0.z) + pParse->sLastToken.n;
  sqlite3AlterFinishAddColumn(pParse, &yymsp[-1].minor.yy0);
}
#line 3723 "parse.c"
        break;
      case 253: /* add_column_fullname ::= fullname */
#line 1498 "parse.y"
{
  disableLookaside(pParse);
  sqlite3AlterBeginAddColumn(pParse, yymsp[0].minor.yy3);
}
#line 3731 "parse.c"
        break;
      case 254: /* with ::= */
#line 1512 "parse.y"
{yymsp[1].minor.yy211 = 0;}
#line 3736 "parse.c"
        break;
      case 255: /* with ::= WITH wqlist */
#line 1514 "parse.y"
{ yymsp[-1].minor.yy211 = yymsp[0].minor.yy211; }
#line 3741 "parse.c"
        break;
      case 256: /* with ::= WITH RECURSIVE wqlist */
#line 1515 "parse.y"
{ yymsp[-2].minor.yy211 = yymsp[0].minor.yy211; }
#line 3746 "parse.c"
        break;
      case 257: /* wqlist ::= nm eidlist_opt AS LP select RP */
#line 1517 "parse.y"
{
  yymsp[-5].minor.yy211 = sqlite3WithAdd(pParse, 0, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy258, yymsp[-1].minor.yy387); /*A-overwrites-X*/
}
#line 3753 "parse.c"
        break;
      case 258: /* wqlist ::= wqlist COMMA nm eidlist_opt AS LP select RP */
#line 1520 "parse.y"
{
  yymsp[-7].minor.yy211 = sqlite3WithAdd(pParse, yymsp[-7].minor.yy211, &yymsp[-5].minor.yy0, yymsp[-4].minor.yy258, yymsp[-1].minor.yy387);
}
#line 3760 "parse.c"
        break;
      default:
      /* (259) input ::= ecmd */ yytestcase(yyruleno==259);
      /* (260) explain ::= */ yytestcase(yyruleno==260);
      /* (261) cmdx ::= cmd (OPTIMIZED OUT) */ assert(yyruleno!=261);
      /* (262) trans_opt ::= */ yytestcase(yyruleno==262);
      /* (263) trans_opt ::= TRANSACTION */ yytestcase(yyruleno==263);
      /* (264) trans_opt ::= TRANSACTION nm */ yytestcase(yyruleno==264);
      /* (265) savepoint_opt ::= SAVEPOINT */ yytestcase(yyruleno==265);
      /* (266) savepoint_opt ::= */ yytestcase(yyruleno==266);
      /* (267) cmd ::= create_table create_table_args */ yytestcase(yyruleno==267);
      /* (268) columnlist ::= columnlist COMMA columnname carglist */ yytestcase(yyruleno==268);
      /* (269) columnlist ::= columnname carglist */ yytestcase(yyruleno==269);
      /* (270) nm ::= ID|INDEXED */ yytestcase(yyruleno==270);
      /* (271) nm ::= JOIN_KW */ yytestcase(yyruleno==271);
      /* (272) typetoken ::= typename */ yytestcase(yyruleno==272);
      /* (273) typename ::= ID|STRING */ yytestcase(yyruleno==273);
      /* (274) signed ::= plus_num (OPTIMIZED OUT) */ assert(yyruleno!=274);
      /* (275) signed ::= minus_num (OPTIMIZED OUT) */ assert(yyruleno!=275);
      /* (276) carglist ::= carglist ccons */ yytestcase(yyruleno==276);
      /* (277) carglist ::= */ yytestcase(yyruleno==277);
      /* (278) ccons ::= NULL onconf */ yytestcase(yyruleno==278);
      /* (279) conslist_opt ::= COMMA conslist */ yytestcase(yyruleno==279);
      /* (280) conslist ::= conslist tconscomma tcons */ yytestcase(yyruleno==280);
      /* (281) conslist ::= tcons (OPTIMIZED OUT) */ assert(yyruleno!=281);
      /* (282) tconscomma ::= */ yytestcase(yyruleno==282);
      /* (283) defer_subclause_opt ::= defer_subclause (OPTIMIZED OUT) */ assert(yyruleno!=283);
      /* (284) resolvetype ::= raisetype (OPTIMIZED OUT) */ assert(yyruleno!=284);
      /* (285) selectnowith ::= oneselect (OPTIMIZED OUT) */ assert(yyruleno!=285);
      /* (286) oneselect ::= values */ yytestcase(yyruleno==286);
      /* (287) sclp ::= selcollist COMMA */ yytestcase(yyruleno==287);
      /* (288) as ::= ID|STRING */ yytestcase(yyruleno==288);
      /* (289) expr ::= term (OPTIMIZED OUT) */ assert(yyruleno!=289);
      /* (290) exprlist ::= nexprlist */ yytestcase(yyruleno==290);
      /* (291) nmnum ::= plus_num (OPTIMIZED OUT) */ assert(yyruleno!=291);
      /* (292) nmnum ::= STRING */ yytestcase(yyruleno==292);
      /* (293) nmnum ::= nm */ yytestcase(yyruleno==293);
      /* (294) nmnum ::= ON */ yytestcase(yyruleno==294);
      /* (295) nmnum ::= DELETE */ yytestcase(yyruleno==295);
      /* (296) nmnum ::= DEFAULT */ yytestcase(yyruleno==296);
      /* (297) plus_num ::= INTEGER|FLOAT */ yytestcase(yyruleno==297);
      /* (298) foreach_clause ::= */ yytestcase(yyruleno==298);
      /* (299) foreach_clause ::= FOR EACH ROW */ yytestcase(yyruleno==299);
      /* (300) trnm ::= nm */ yytestcase(yyruleno==300);
      /* (301) tridxby ::= */ yytestcase(yyruleno==301);
      /* (302) kwcolumn_opt ::= */ yytestcase(yyruleno==302);
      /* (303) kwcolumn_opt ::= COLUMNKW */ yytestcase(yyruleno==303);
        break;
/********** End reduce actions ************************************************/
  };
  assert( yyruleno<sizeof(yyRuleInfo)/sizeof(yyRuleInfo[0]) );
  yygoto = yyRuleInfo[yyruleno].lhs;
  yysize = yyRuleInfo[yyruleno].nrhs;
  yyact = yy_find_reduce_action(yymsp[-yysize].stateno,(YYCODETYPE)yygoto);
  if( yyact <= YY_MAX_SHIFTREDUCE ){
    if( yyact>YY_MAX_SHIFT ){
      yyact += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
    }
    yymsp -= yysize-1;
    yypParser->yytos = yymsp;
    yymsp->stateno = (YYACTIONTYPE)yyact;
    yymsp->major = (YYCODETYPE)yygoto;
    yyTraceShift(yypParser, yyact);
  }else{
    assert( yyact == YY_ACCEPT_ACTION );
    yypParser->yytos -= yysize;
    yy_accept(yypParser);
  }
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sFail!\n",yyTracePrompt);
  }
#endif
  while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
/************ Begin %parse_failure code ***************************************/
/************ End %parse_failure code *****************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  yyParser *yypParser,           /* The parser */
  int yymajor,                   /* The major type of the error token */
  sqlite3ParserTOKENTYPE yyminor         /* The minor type of the error token */
){
  sqlite3ParserARG_FETCH;
#define TOKEN yyminor
/************ Begin %syntax_error code ****************************************/
#line 32 "parse.y"

  UNUSED_PARAMETER(yymajor);  /* Silence some compiler warnings */
  assert( TOKEN.z[0] );  /* The tokenizer always gives us a token */
  sqlite3ErrorMsg(pParse, "near \"%T\": syntax error", &TOKEN);
#line 3869 "parse.c"
/************ End %syntax_error code ******************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  yyParser *yypParser           /* The parser */
){
  sqlite3ParserARG_FETCH;
#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sAccept!\n",yyTracePrompt);
  }
#endif
#ifndef YYNOERRORRECOVERY
  yypParser->yyerrcnt = -1;
#endif
  assert( yypParser->yytos==yypParser->yystack );
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
/*********** Begin %parse_accept code *****************************************/
/*********** End %parse_accept code *******************************************/
  sqlite3ParserARG_STORE; /* Suppress warning about unused %extra_argument variable */
}

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "sqlite3ParserAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
void sqlite3Parser(
  void *yyp,                   /* The parser */
  int yymajor,                 /* The major token code number */
  sqlite3ParserTOKENTYPE yyminor       /* The value for the token */
  sqlite3ParserARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  unsigned int yyact;   /* The parser action. */
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  int yyendofinput;     /* True if we are at the end of input */
#endif
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser *yypParser;  /* The parser */

  yypParser = (yyParser*)yyp;
  assert( yypParser->yytos!=0 );
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  yyendofinput = (yymajor==0);
#endif
  sqlite3ParserARG_STORE;

#ifndef NDEBUG
  if( yyTraceFILE ){
    fprintf(yyTraceFILE,"%sInput '%s'\n",yyTracePrompt,yyTokenName[yymajor]);
  }
#endif

  do{
    yyact = yy_find_shift_action(yypParser,(YYCODETYPE)yymajor);
    if( yyact <= YY_MAX_SHIFTREDUCE ){
      yy_shift(yypParser,yyact,yymajor,yyminor);
#ifndef YYNOERRORRECOVERY
      yypParser->yyerrcnt--;
#endif
      yymajor = YYNOCODE;
    }else if( yyact <= YY_MAX_REDUCE ){
      yy_reduce(yypParser,yyact-YY_MIN_REDUCE);
    }else{
      assert( yyact == YY_ERROR_ACTION );
      yyminorunion.yy0 = yyminor;
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifndef NDEBUG
      if( yyTraceFILE ){
        fprintf(yyTraceFILE,"%sSyntax Error!\n",yyTracePrompt);
      }
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        yy_syntax_error(yypParser,yymajor,yyminor);
      }
      yymx = yypParser->yytos->major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
#ifndef NDEBUG
        if( yyTraceFILE ){
          fprintf(yyTraceFILE,"%sDiscard input token %s\n",
             yyTracePrompt,yyTokenName[yymajor]);
        }
#endif
        yy_destructor(yypParser, (YYCODETYPE)yymajor, &yyminorunion);
        yymajor = YYNOCODE;
      }else{
        while( yypParser->yytos >= yypParser->yystack
            && yymx != YYERRORSYMBOL
            && (yyact = yy_find_reduce_action(
                        yypParser->yytos->stateno,
                        YYERRORSYMBOL)) >= YY_MIN_REDUCE
        ){
          yy_pop_parser_stack(yypParser);
        }
        if( yypParser->yytos < yypParser->yystack || yymajor==0 ){
          yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
          yypParser->yyerrcnt = -1;
#endif
          yymajor = YYNOCODE;
        }else if( yymx!=YYERRORSYMBOL ){
          yy_shift(yypParser,yyact,YYERRORSYMBOL,yyminor);
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      yy_syntax_error(yypParser,yymajor, yyminor);
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      yymajor = YYNOCODE;
      
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        yy_syntax_error(yypParser,yymajor, yyminor);
      }
      yypParser->yyerrcnt = 3;
      yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
        yypParser->yyerrcnt = -1;
#endif
      }
      yymajor = YYNOCODE;
#endif
    }
  }while( yymajor!=YYNOCODE && yypParser->yytos>yypParser->yystack );
#ifndef NDEBUG
  if( yyTraceFILE ){
    yyStackEntry *i;
    char cDiv = '[';
    fprintf(yyTraceFILE,"%sReturn. Stack=",yyTracePrompt);
    for(i=&yypParser->yystack[1]; i<=yypParser->yytos; i++){
      fprintf(yyTraceFILE,"%c%s", cDiv, yyTokenName[i->major]);
      cDiv = ' ';
    }
    fprintf(yyTraceFILE,"]\n");
  }
#endif
  return;
}
