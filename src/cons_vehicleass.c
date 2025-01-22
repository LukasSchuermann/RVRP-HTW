/**@file   cons_vehicleass.c
 * @brief  constraint handler for vehicle assignment branching decisions
 * @author Lukas Schürmann, University Bonn
 */
#include <assert.h>

#include "cons_vehicleass.h"
#include "probdata_vrp.h"
#include "vardata_vrp.h"

#define CONSHDLR_NAME          "vehicleass"
#define CONSHDLR_DESC          "stores the local branching decisions"
#define CONSHDLR_ENFOPRIORITY         0 /**< priority of the constraint handler for constraint enforcing */
#define CONSHDLR_CHECKPRIORITY  9999999 /**< priority of the constraint handler for checking feasibility */
#define CONSHDLR_PROPFREQ             1 /**< frequency for propagating domains; zero means only preprocessing propagation */
#define CONSHDLR_EAGERFREQ            1 /**< frequency for using all instead of only the useful constraints in separation,
                                         *   propagation and enforcement, -1 for no eager evaluations, 0 for first only */
#define CONSHDLR_DELAYPROP        FALSE /**< should propagation method be delayed, if other propagators found reductions? */
#define CONSHDLR_NEEDSCONS         TRUE /**< should the constraint handler be skipped, if no constraints are available? */

#define CONSHDLR_PROP_TIMING       SCIP_PROPTIMING_BEFORELP

/**@} */

/*
 * Data structures
 */


/** Constraint data for  \ref cons_VehicleAss.c "VehicleAss" constraints */
struct SCIP_ConsData
{
    int                   customer;           /**< corresponding customer */
    int                   day;                /**< corresponding day */
    CONSTYPEVA              type;               /**< stores whether customers gets enforced or prohibited on that day */
    int                   npropagatedvars;    /**< number of variables that existed, the last time, the related node was
                                              *   propagated, used to determine whether the constraint should be
                                              *   repropagated*/
    int                   npropagations;      /**< stores the number propagations runs of this constraint */
    unsigned int          propagated:1;       /**< is constraint already propagated? */
    SCIP_NODE*            node;               /**< the node in the B&B-tree at which the cons is sticking */
};

/**@name Local methods
 *
 * @{
 */

/** create constraint data */
static
SCIP_RETCODE consdataCreate(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONSDATA**       consdata,           /**< pointer to store the constraint data */
        int                   customer,           /**< corresponding customer */
        int                   day,                /**< corresponding day */
        CONSTYPEVA              type,               /**< stores whether arc gets enforced or prohibited */
        SCIP_NODE*            node                /**< the node in the B&B-tree at which the cons is sticking */
)
{
    assert( scip != NULL );
    assert( consdata != NULL );
    assert( customer >= 0 );
    assert( day >= 0 );
    assert( type == VA_ENFORCE || type == VA_PROHIBIT );


    SCIP_CALL( SCIPallocBlockMemory(scip, consdata) );

    (*consdata)->customer = customer;
    (*consdata)->day = day;
    (*consdata)->type = type;
    (*consdata)->npropagatedvars = 0;
    (*consdata)->npropagations = 0;
    (*consdata)->propagated = FALSE;
    (*consdata)->node = node;

    return SCIP_OKAY;
}

/** display constraints */
static
void consdataPrint(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONSDATA*        consdata,           /**< constraint data */
        FILE*                 file                /**< file stream */
)
{
    SCIP_PROBDATA* probdata;

    probdata = SCIPgetProbData(scip);
    assert(probdata != NULL);

    SCIPinfoMessage(scip, file, "%s(%d,%d) at node %lld\n",
                    consdata->type == VA_PROHIBIT ? "prohibit" : "enforce",
                    consdata->customer, consdata->day, SCIPnodeGetNumber(consdata->node) );
}

/** Checks if (a): customer lies in the tour of the variable
 *            (b): tour takes place on the given day
 * @return  0   if (a) or  (b) is FALSE
 *          1   if (a) xor (b) is TRUE
 *          2   if (a) and (b) is TRUE
 * */
static
int customerAndDayInVar(
        SCIP_VAR*             var,                /**< variable to check */
        int                   customer,           /**< customer to check */
        int                   day                 /**< day to check */
)
{
    SCIP_VARDATA* varData = SCIPvarGetData(var);
    assert(varData != NULL);

    for(int i = 0; i < varData->tourlength; i++)
    {
        int u = varData->customertour[i];
        assert(u >= 0);
        if(u == customer) // case 2
        {
            if(varData->day == day)
            {
                return 2;
            }
            else
            {
                return 1;
            }
        }
    }
    if(varData->day == day)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/** fixes a variable to zero if the corresponding arcs are not valid for this constraint/node (due to branching) */
static
SCIP_RETCODE checkVariable(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONSDATA*        consdata,           /**< constraint data */
        SCIP_VAR*             var,                /**< variables to check  */
        int*                  nfixedvars,         /**< pointer to store the number of fixed variables */
        SCIP_Bool*            cutoff              /**< pointer to store if a cutoff was detected */
)
{
    int arcStatus;
    CONSTYPEVA type;

    SCIP_Bool fixed;
    SCIP_Bool infeasible;

    assert(scip != NULL);
    assert(consdata != NULL);
    assert(var != NULL);
    assert(nfixedvars != NULL);
    assert(cutoff != NULL);

    /* if variables is locally fixed to zero continue */
    if( SCIPvarGetUbLocal(var) < 0.5 )
        return SCIP_OKAY;

    /* check if the tour which corresponds to the variable is feasible for this constraint */
    arcStatus = customerAndDayInVar(var, consdata->customer, consdata->day);

    /** arcStatus = 0 (neither the day nor the customer fit to the variable) -> variable allowed
     *  arcStatus = 1 (either the day or the customer fit to the variable) -> variable 0-fixed if customers gets enforced on that day
     *  arcStatus = 2 (both the day and the customer fit to the variable) -> variable 0-fixed if customers gets prohibited for that day */

    if(arcStatus == 0) return SCIP_OKAY;
    type = consdata->type;
    if( (type == VA_PROHIBIT && arcStatus == 2) || (type == VA_ENFORCE && arcStatus == 1) )
    {
        SCIP_CALL( SCIPfixVar(scip, var, 0.0, &infeasible, &fixed) );

        if( infeasible )
        {
            assert( SCIPvarGetLbLocal(var) > 0.5 );
            SCIPdebugMsg(scip, "-> cutoff\n");
            (*cutoff) = TRUE;
        }
        else
        {
            assert(fixed);
            (*nfixedvars)++;
        }
    }

    return SCIP_OKAY;
}

/** fixes variables to zero if the corresponding tours are not valid for this constraint/node (due to branching) */
static
SCIP_RETCODE consdataFixVariables(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONSDATA*        consdata,           /**< constraint data */
        SCIP_VAR**            vars,               /**< generated variables */
        int                   nvars,              /**< number of generated variables */
        SCIP_RESULT*          result              /**< pointer to store the result of the fixing */
)
{
    int nfixedvars;
    int v;
    SCIP_Bool cutoff;

    nfixedvars = 0;
    cutoff = FALSE;

    SCIPdebugMsg(scip, "check variables %d to %d\n", consdata->npropagatedvars, nvars);

    for( v = consdata->npropagatedvars; v < nvars && !cutoff; ++v )
    {
        SCIP_CALL( checkVariable(scip, consdata, vars[v], &nfixedvars, &cutoff) );
    }

    SCIPdebugMsg(scip, "fixed %d variables locally\n", nfixedvars);

    if( cutoff )
        *result = SCIP_CUTOFF;
    else if( nfixedvars > 0 )
        *result = SCIP_REDUCEDDOM;

    return SCIP_OKAY;
}

/** check if all variables are valid for the given consdata */
#ifndef NDEBUG
static
SCIP_Bool consdataCheck(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_PROBDATA*        probdata,           /**< problem data */
        SCIP_CONSDATA*        consdata,           /**< constraint data */
        SCIP_Bool             beforeprop          /**< is this check performed before propagation? */
)
{
    int nvars;

    int arcStatus;
    CONSTYPEVA type;

    int v;
    nvars = (beforeprop ? consdata->npropagatedvars : probdata->nvars);
    assert(nvars <= probdata->nvars);

    for( v = 0; v < nvars; ++v )
    {
        SCIP_VAR* var = probdata->vars[v];

        /* if variables is locally fixed to zero continue */
        if( SCIPvarGetUbLocal(var) < 0.5 )
            continue;

        arcStatus = customerAndDayInVar(var, consdata->customer, consdata->day);

        if(arcStatus == 0) return SCIP_OKAY;

        type = consdata->type;
        if( (type == VA_PROHIBIT && arcStatus == 2) || (type == VA_ENFORCE && arcStatus == 1) )
        {
            SCIPdebug( consdataPrint(scip, consdata, NULL) );
            SCIPdebug( SCIPprintVar(scip, var, NULL) );
            SCIPprintVar(scip, var, NULL);
            return FALSE;
        }
    }

    return TRUE;
}
#endif

/** frees VehicleAss constraint data */
static
SCIP_RETCODE consdataFree(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONSDATA**       consdata            /**< pointer to the constraint data */
)
{
    assert(consdata != NULL);
    assert(*consdata != NULL);

    SCIPfreeBlockMemory(scip, consdata);

    return SCIP_OKAY;
}
/**@} */

/**@name Callback methods
 *
 * @{
 */

/** frees specific constraint data */
static
SCIP_DECL_CONSDELETE(consDeleteVehicleAss)
{  /*lint --e{715}*/
            assert(conshdlr != NULL);
    assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    assert(consdata != NULL);
    assert(*consdata != NULL);

    /* free VehicleAss constraint */
    SCIP_CALL( consdataFree(scip, consdata) );

    return SCIP_OKAY;
}

/** transforms constraint data into data belonging to the transformed problem */
static
SCIP_DECL_CONSTRANS(consTransVehicleAss)
{
    SCIP_CONSDATA* sourcedata;
    SCIP_CONSDATA* targetdata;

    assert(conshdlr != NULL);
    assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    assert(SCIPgetStage(scip) == SCIP_STAGE_TRANSFORMING);
    assert(sourcecons != NULL);
    assert(targetcons != NULL);

    sourcedata = SCIPconsGetData(sourcecons);
    assert(sourcedata != NULL);

    /* create constraint data for target constraint */
    SCIP_CALL( consdataCreate(scip, &targetdata,
    sourcedata->customer, sourcedata->day, sourcedata->type, sourcedata->node) );

    /* create target constraint */
    SCIP_CALL( SCIPcreateCons(scip, targetcons, SCIPconsGetName(sourcecons), conshdlr, targetdata,
    SCIPconsIsInitial(sourcecons), SCIPconsIsSeparated(sourcecons), SCIPconsIsEnforced(sourcecons),
    SCIPconsIsChecked(sourcecons), SCIPconsIsPropagated(sourcecons),
    SCIPconsIsLocal(sourcecons), SCIPconsIsModifiable(sourcecons),
    SCIPconsIsDynamic(sourcecons), SCIPconsIsRemovable(sourcecons), SCIPconsIsStickingAtNode(sourcecons)) );

    return SCIP_OKAY;
}

/** constraint enforcing method of constraint handler for LP solutions */
#define consEnfolpVehicleAss NULL

/** constraint enforcing method of constraint handler for pseudo solutions */
#define consEnfopsVehicleAss NULL

/** feasibility check method of constraint handler for integral solutions */
#define consCheckVehicleAss NULL

/** domain propagation method of constraint handler */
static
SCIP_DECL_CONSPROP(consPropVehicleAss)
{
    SCIP_PROBDATA* probData;
    SCIP_CONSDATA* consdata;

    int c;

    assert(scip != NULL);
    assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    assert(result != NULL);

    SCIPdebugMsg(scip, "propagation constraints of constraint handler <"CONSHDLR_NAME">\n");

    probData = SCIPgetProbData(scip);
    assert(probData != NULL);


    *result = SCIP_DIDNOTFIND;
    for( c = 0; c < nconss; ++c )
    {
        consdata = SCIPconsGetData(conss[c]);

        /* check if all previously generated variables are valid for this constraint */
        assert( consdataCheck(scip, probData, consdata, TRUE) );

#ifndef NDEBUG
        {
            /* check if there are no equal consdatas */
            SCIP_CONSDATA* consdata2;
            int i;

            for( i = c+1; i < nconss; ++i )
            {
                consdata2 = SCIPconsGetData(conss[i]);

                assert( !(consdata->customer == consdata2->customer && consdata->type == VA_ENFORCE && consdata2->type == VA_ENFORCE));
                if(consdata->customer == consdata2->customer && consdata->day == consdata2->day)
                {
//                    cout << "customer "<<consdata->customer<<" day "<<consdata->day<< endl;
//                    cout << "parent: " <<SCIPnodeGetNumber(SCIPnodeGetParent(SCIPgetCurrentNode(scip))) << endl;
//                    cout << "type1: " << consdata->type << " vs type2: " << consdata2->type << endl;
                }
                assert( !(consdata->customer == consdata2->customer && consdata->day == consdata2->day) );
            }
        }
#endif
        if( !consdata->propagated )
        {
            SCIPdebugMsg(scip, "propagate constraint <%s> ", SCIPconsGetName(conss[c]));
            SCIPdebug( consdataPrint(scip, consdata, nullptr) );
            SCIP_CALL( consdataFixVariables(scip, consdata, probData->vars, probData->nvars, result) );
            consdata->npropagations++;

            if( *result != SCIP_CUTOFF )
            {
                consdata->propagated = TRUE;
                consdata->npropagatedvars = probData->nvars;
            }
            else
                break;
        }

        /* check if constraint is completely propagated */
        assert( consdataCheck(scip, probData, consdata, FALSE) );
    }

    return SCIP_OKAY;
}

/** variable rounding lock method of constraint handler */
#define consLockVehicleAss NULL

/** constraint activation notification method of constraint handler */
static
SCIP_DECL_CONSACTIVE(consActiveVehicleAss)
{
    SCIP_CONSDATA* consdata;
    SCIP_PROBDATA* probdata;

    assert(scip != NULL);
    assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    assert(cons != NULL);

    probdata = SCIPgetProbData(scip);
    assert(probdata != NULL);

    consdata = SCIPconsGetData(cons);
    assert(consdata != NULL);
    assert(consdata->npropagatedvars <= probdata->nvars);

    SCIPdebugMsg(scip, "activate constraint <%s> at node <%"SCIP_LONGINT_FORMAT"> in depth <%d>: ",
    SCIPconsGetName(cons), SCIPnodeGetNumber(consdata->node), SCIPnodeGetDepth(consdata->node));
    SCIPdebug( consdataPrint(scip, consdata, NULL) );

    if( consdata->npropagatedvars != probdata->nvars )
    {
        SCIPdebugMsg(scip, "-> mark constraint to be repropagated\n");
        consdata->propagated = FALSE;
        SCIP_CALL( SCIPrepropagateNode(scip, consdata->node) );
    }

    return SCIP_OKAY;
}

/** constraint deactivation notification method of constraint handler */
static
SCIP_DECL_CONSDEACTIVE(consDeactiveVehicleAss)
{
    SCIP_CONSDATA* consdata;
    SCIP_PROBDATA* probdata;

    assert(scip != NULL);
    assert(strcmp(SCIPconshdlrGetName(conshdlr), CONSHDLR_NAME) == 0);
    assert(cons != NULL);

    consdata = SCIPconsGetData(cons);
    assert(consdata != NULL);
    assert(consdata->propagated || SCIPgetNChildren(scip) == 0);

    probdata = SCIPgetProbData(scip);
    assert(probdata != NULL);

    SCIPdebugMsg(scip, "deactivate constraint <%s> at node <%"SCIP_LONGINT_FORMAT"> in depth <%d>: ",
    SCIPconsGetName(cons), SCIPnodeGetNumber(consdata->node), SCIPnodeGetDepth(consdata->node));
    SCIPdebug( consdataPrint(scip, consdata, NULL) );

    /* set the number of propagated variables to current number of variables is SCIP */
    consdata->npropagatedvars = probdata->nvars;

    return SCIP_OKAY;
}

/** constraint display method of constraint handler */
static
SCIP_DECL_CONSPRINT(consPrintVehicleAss)
{
    SCIP_CONSDATA*  consdata;

    consdata = SCIPconsGetData(cons);
    assert(consdata != NULL);

    consdataPrint(scip, consdata, file);

    return SCIP_OKAY;
}

/**@} */

/**@name Interface methods
 *
 * @{
 */

/** creates the vehicle assignment Conshdlr and includes it in SCIP */
SCIP_RETCODE SCIPincludeConshdlrVehicleAss(
        SCIP*              scip             /**< SCIP data structure */
)
{
    SCIP_CONSHDLRDATA* conshdlrdata = NULL;
    SCIP_CONSHDLR* conshdlr = NULL;

    /* include constraint handler */
    SCIP_CALL( SCIPincludeConshdlrBasic(scip, &conshdlr, CONSHDLR_NAME, CONSHDLR_DESC,
                                        CONSHDLR_ENFOPRIORITY, CONSHDLR_CHECKPRIORITY, CONSHDLR_EAGERFREQ, CONSHDLR_NEEDSCONS,
                                        consEnfolpVehicleAss, consEnfopsVehicleAss, consCheckVehicleAss, consLockVehicleAss,
                                        conshdlrdata) );
    assert(conshdlr != NULL);

    SCIP_CALL( SCIPsetConshdlrDelete(scip, conshdlr, consDeleteVehicleAss) );
    SCIP_CALL( SCIPsetConshdlrTrans(scip, conshdlr, consTransVehicleAss) );
    SCIP_CALL( SCIPsetConshdlrProp(scip, conshdlr, consPropVehicleAss, CONSHDLR_PROPFREQ, CONSHDLR_DELAYPROP,
                                   CONSHDLR_PROP_TIMING) );
    SCIP_CALL( SCIPsetConshdlrActive(scip, conshdlr, consActiveVehicleAss) );
    SCIP_CALL( SCIPsetConshdlrDeactive(scip, conshdlr, consDeactiveVehicleAss) );
    SCIP_CALL( SCIPsetConshdlrPrint(scip, conshdlr, consPrintVehicleAss) );

    return SCIP_OKAY;
}

/** creates an vehicle assignment constraint */
SCIP_RETCODE SCIPcreateConsVehicleAss(
        SCIP*               scip,                /**< SCIP data structure */
        SCIP_CONS**         cons,                /**< pointer to hold the created constraint */
        const char*         name,                /**< name of the constraint */
        int                 tail,                /**< tail of the arc */
        int                 head,                /**< head of the arc */
        CONSTYPEVA            type,                /**< stores whether arc gets enforced or prohibited */
        SCIP_NODE*          node,                /**< the node in the B&B-tree at which the cons is sticking */
        SCIP_Bool           local                /**< is constraint only valid locally? */
){
    SCIP_CONSHDLR* conshdlr;
    SCIP_CONSDATA* consdata;

    /* find the VehicleAss constraint handler */
    conshdlr = SCIPfindConshdlr(scip, CONSHDLR_NAME);
    if( conshdlr == NULL )
    {
        SCIPerrorMessage("VehicleAss constraint handler not found\n");
        return SCIP_PLUGINNOTFOUND;
    }

    /* create the constraint specific data */
    SCIP_CALL( consdataCreate(scip, &consdata, tail, head, type, node) );

    /* create constraint */
    SCIP_CALL( SCIPcreateCons(scip, cons, name, conshdlr, consdata, FALSE, FALSE, FALSE, FALSE, TRUE,
                              local, FALSE, FALSE, FALSE, TRUE) );

    SCIPdebugMsg(scip, "created constraint: ");
    SCIPdebug( consdataPrint(scip, consdata, NULL) );

    return SCIP_OKAY;
}

/** returns tail of the arc */
int SCIPgetCustomerVehicleAss(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONS*            cons                /**< VehicleAss constraint */
)
{
    SCIP_CONSDATA* consdata;

    assert(cons != NULL);

    consdata = SCIPconsGetData(cons);
    assert(consdata != NULL);

    return consdata->customer;
}

/** returns head of the arc */
int SCIPgetDayVehicleAss(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONS*            cons                /**< VehicleAss constraint */
)
{
    SCIP_CONSDATA* consdata;

    assert(cons != NULL);

    consdata = SCIPconsGetData(cons);
    assert(consdata != NULL);

    return consdata->day;
}

/** return constraint type PROHIBIT or ENFORCE */
CONSTYPEVA SCIPgetTypeVehicleAss(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONS*            cons                /**< VehicleAss constraint */
)
{
    SCIP_CONSDATA* consdata;

    assert(cons != NULL);

    consdata = SCIPconsGetData(cons);
    assert(consdata != NULL);

    return consdata->type;
}

/** returns number of propagated variables for cons */
int SCIPgetNPropagatedVA(
        SCIP*               scip,
        SCIP_CONS*          cons
        ){
    SCIP_CONSDATA* consdata;

    assert(cons != NULL);

    consdata = SCIPconsGetData(cons);
    assert(consdata != NULL);

    return consdata->npropagatedvars;
}

/**@} */
