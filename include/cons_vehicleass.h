/**@file   cons_vehicleass.h
 * @brief  constraint handler for vehicle assignment branching decisions
 * @author Lukas Schürmann, University Bonn
 */

#ifndef VRP_CONSHDLRVEHICLEASS_H
#define VRP_CONSHDLRVEHICLEASS_H

#include "scip/scip.h"

enum ConsTypeVA
{
    VA_PROHIBIT = 0,
    VA_ENFORCE = 1
};
typedef enum ConsTypeVA CONSTYPEVA;

/** creates the vehicle assignment Conshdlr and includes it in SCIP */
SCIP_RETCODE SCIPincludeConshdlrVehicleAss(
        SCIP*               scip
        );

/** creates an vehicle assignment constraint */
SCIP_RETCODE SCIPcreateConsVehicleAss(
        SCIP*               scip,
        SCIP_CONS**         cons,
        const char*         name,
        int                 tail,
        int                 head,
        CONSTYPEVA          type,
        SCIP_NODE*          node,
        SCIP_Bool           local
        );

/** returns customer of the constraint */
int SCIPgetCustomerVehicleAss(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONS*            cons                /**< samediff constraint */
);

/** returns day of the constraint */
int SCIPgetDayVehicleAss(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONS*            cons                /**< samediff constraint */
);

/** return constraint type PROHIBIT or ENFORCE */
CONSTYPEVA SCIPgetTypeVehicleAss(
        SCIP*                 scip,               /**< SCIP data structure */
        SCIP_CONS*            cons                /**< samediff constraint */
);

/** returns number of propagated variables for cons */
int SCIPgetNPropagatedVA(
        SCIP*               scip,
        SCIP_CONS*          cons
);

#endif
