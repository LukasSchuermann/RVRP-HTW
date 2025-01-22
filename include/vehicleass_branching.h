/**@file   vehicleass_branching.h
 * @brief  branching rule for the VRP
 * @author Lukas Schürmann, University Bonn
 */


#ifndef VRP_BRANCHINGRULE_VEHICLEASS_H
#define VRP_BRANCHINGRULE_VEHICLEASS_H

#include "scip/scip.h"
#include "tools_data.h"

/** creates the vehicle assignment branching rule and includes it in SCIP */
SCIP_RETCODE SCIPincludeBranchruleVehicleAss(
        SCIP*                   scip,                /**< SCIP data structure */
        int                     priority
);


#endif //VRP_BRANCHINGRULE_VEHICLEASS_H
