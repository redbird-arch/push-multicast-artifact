/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "mem/ruby/network/garnet/RoutingUnit.hh"

#include "base/cast.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/Router.hh"
#include "mem/ruby/slicc_interface/Message.hh"

RoutingUnit::RoutingUnit(Router *router)
{
    m_router = router;
    m_routing_table.clear();
    m_weight_table.clear();
}

void
RoutingUnit::addRoute(std::vector<NetDest>& routing_table_entry)
{
    if (routing_table_entry.size() > m_routing_table.size()) {
        m_routing_table.resize(routing_table_entry.size());
    }
    for (int v = 0; v < routing_table_entry.size(); v++) {
        m_routing_table[v].push_back(routing_table_entry[v]);
    }
}

void
RoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

bool
RoutingUnit::supportsVnet(int vnet, std::vector<int> sVnets)
{
    // If all vnets are supported, return true
    if (sVnets.size() == 0) {
        return true;
    }

    // Find the vnet in the vector, return true
    if (std::find(sVnets.begin(), sVnets.end(), vnet) != sVnets.end()) {
        return true;
    }

    // Not supported vnet
    return false;
}

/*
 * This is the default routing algorithm in garnet.
 * The routing table is populated during topology creation.
 * Routes can be biased via weight assignments in the topology file.
 * Correct weight assignments are critical to provide deadlock avoidance.
 */
int
RoutingUnit::lookupRoutingTable(int vnet, NetDest msg_destination)
{
    // First find all possible output link candidates
    // For ordered vnet, just choose the first
    // (to make sure different packets don't choose different routes)
    // For unordered vnet, randomly choose any of the links
    // To have a strict ordering between links, they should be given
    // different weights in the topology file

    int output_link = -1;
    int min_weight = INFINITE_;
    std::vector<int> output_link_candidates;
    int num_candidates = 0;

    // Identify the minimum weight among the candidate output links
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

        if (m_weight_table[link] <= min_weight)
            min_weight = m_weight_table[link];
        }
    }

    // Collect all candidate output links with this minimum weight
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

            if (m_weight_table[link] == min_weight) {
                num_candidates++;
                output_link_candidates.push_back(link);
            }
        }
    }

    if (output_link_candidates.size() == 0) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    // Randomly select any candidate output link
    int candidate = 0;
    if (!(m_router->get_net_ptr())->isVNetOrdered(vnet))
        candidate = rand() % num_candidates;

    output_link = output_link_candidates.at(candidate);
    return output_link;
}


void
RoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx;
    m_inports_idx2dirn[inport_idx]  = inport_dirn;
}

void
RoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx;
    m_outports_idx2dirn[outport_idx]  = outport_dirn;
}

// outportCompute() is called by the InputUnit
// It calls the routing table by default.
// A template for adaptive topology-specific routing algorithm
// implementations using port directions rather than a static routing
// table is provided here.

int
RoutingUnit::outportCompute(RouteInfo route, int inport,
                            PortDirection inport_dirn,
                            int vnet)
{
    int outport = -1;

    if (route.dest_router == m_router->get_id()) {

        // Multiple NIs may be connected to this router,
        // all with output port direction = "Local"
        // Get exact outport id from table
        outport = lookupRoutingTable(route.vnet, route.net_dest);
        return outport;
    }

    // Routing Algorithm set in GarnetNetwork.py
    // Can be over-ridden from command line using --routing-algorithm = 1
    RoutingAlgorithm routing_algorithm =
        (RoutingAlgorithm) m_router->get_net_ptr()->getRoutingAlgorithm();

    switch (routing_algorithm) {
        case TABLE_:  outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
        case XY_:     outport =
            outportComputeXY(route, inport, inport_dirn); break;
        // any custom algorithm
        case CUSTOM_: outport =
            outportComputeCustom(route, inport, inport_dirn); break;
        case XY_YX_:  outport =
            outportComputeXYYX(route, inport, inport_dirn, vnet); break;
        default: outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
    }

    assert(outport != -1);
    return outport;
}

// XY routing implemented using port directions
// Only for reference purpose in a Mesh
// By default Garnet uses the routing table
int
RoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    M5_VAR_USED int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops > 0) {
        if (x_dirn) {
            assert(inport_dirn == "Local" || inport_dirn == "West");
            outport_dirn = "East";
        } else {
            assert(inport_dirn == "Local" || inport_dirn == "East");
            outport_dirn = "West";
        }
    } else if (y_hops > 0) {
        if (y_dirn) {
            // "Local" or "South" or "West" or "East"
            assert(inport_dirn != "North");
            outport_dirn = "North";
        } else {
            // "Local" or "North" or "West" or "East"
            assert(inport_dirn != "South");
            outport_dirn = "South";
        }
    } else {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == 0");
    }

    return m_outports_dirn2idx[outport_dirn];
}

// Template for implementing custom routing algorithm
// using port directions. (Example adaptive)
int
RoutingUnit::outportComputeCustom(RouteInfo route,
                                 int inport,
                                 PortDirection inport_dirn)
{
    panic("%s placeholder executed", __FUNCTION__);
}

/*
 * YX routing implemented using port directions for 2D mesh network.
 * By default Garnet uses the routing table.
 */
int
RoutingUnit::outportComputeYX(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    M5_VAR_USED int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (y_hops > 0) {
        if (y_dirn) {
            assert(inport_dirn == "Local" || inport_dirn == "South");
            outport_dirn = "North";
        } else {
            assert(inport_dirn == "Local" || inport_dirn == "North");
            outport_dirn = "South";
        }
    } else if (x_hops > 0) {
        if (x_dirn) {
            // "Local" or "West" or "North" or "South"
            assert(inport_dirn != "East");
            outport_dirn = "East";
        } else {
            // "Local" or "East" or "North" or "South"
            assert(inport_dirn != "West");
            outport_dirn = "West";
        }
    } else {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == 0");
    }

    return m_outports_dirn2idx[outport_dirn];
}

/*
 * XY-YX routing implemented using port directions and virtual network types
 * for 2D mesh network. Control virtual network uses XY routing while data
 * virtual network uses YX routing.
 * By default Garnet uses the routing table.
 */
int
RoutingUnit::outportComputeXYYX(RouteInfo route,
                                int inport,
                                PortDirection inport_dirn,
                                int vnet)
{
    int outport = -1;

    GarnetNetwork *net_ptr = m_router->get_net_ptr();

    if (net_ptr->getCoherenceConstraint() != UNORDERED_) {
        if (vnet == net_ptr->getRequestVnet()) {
            outport = outportComputeXY(route, inport, inport_dirn);
        } else {
            outport = outportComputeYX(route, inport, inport_dirn);
        }
    } else {
        VNET_type vnet_type = net_ptr->get_vnet_type(vnet);
        switch (vnet_type) {
            case CTRL_VNET_:
                outport = outportComputeXY(route, inport, inport_dirn); break;
            case DATA_VNET_:
                outport = outportComputeYX(route, inport, inport_dirn); break;
            default:
                panic("Unknown VNET type: %d\n", vnet_type); break;
        }
    }

    return outport;
}

std::vector<int>
RoutingUnit::multicastOutportsCompute(RouteInfo &route, int inport,
        PortDirection inport_dirn, int vnet)
{
    assert(route.dest_router == -1);
    std::set<int> unique_outport_set;
    std::vector<int> outports;

    assert(route.demandOutports.empty());

    for (int i = 0; i < route.destRouters.size(); i++) {
        RouteInfo single_route = route;

        single_route.dest_router = route.destRouters[i];
        single_route.net_dest = route.netDests[i];

        int outport =
            outportCompute(single_route, inport, inport_dirn, vnet);

        unique_outport_set.insert(outport);
        outports.push_back(outport);
    }

    // Construct new route info
    for (auto outport : unique_outport_set) {
        RouteInfo new_route = route;

        new_route.dest_ni = -1;
        if (route.dest_ni != -1) {
            new_route.dest_ni = route.dest_ni;
        }
        new_route.dest_router = -1;
        new_route.net_dest.clear();
        new_route.destRouters.clear();
        new_route.destNIs.clear();
        new_route.netDests.clear();

        new_route.demandDestNIs = route.demandDestNIs;
        new_route.demandOutports.clear();

        route.outportRouteMap[outport] = new_route;
    }

    // Add new route multicast destinations info
    for (int i = 0; i < outports.size(); i++) {
        int outport = outports[i];

        RouteInfo &new_route = route.outportRouteMap[outport];

        // demand requestor
        if (route.demandDestNIs.find(route.destNIs[i]) !=
                route.demandDestNIs.end()) {
            route.demandOutports.insert(outport);
        }

        new_route.net_dest.addNetDest(route.netDests[i]);
        new_route.destRouters.push_back(route.destRouters[i]);
        new_route.destNIs.push_back(route.destNIs[i]);
        new_route.netDests.push_back(route.netDests[i]);
    }

    // Update to be unicast if it becomes unicast
    for (auto outport: unique_outport_set) {
        RouteInfo &new_route = route.outportRouteMap[outport];

        if (new_route.destNIs.size() == 1) {
            new_route.dest_ni = new_route.destNIs[0];
            new_route.dest_router = new_route.destRouters[0];

            new_route.destNIs.clear();
            new_route.destRouters.clear();
            new_route.netDests.clear();
        }

        new_route.outportRouteMap.clear();
    }

    std::vector<int> unique_outports;
    for (auto outport: unique_outport_set) {
        if (route.demandOutports.find(outport) != route.demandOutports.end())
            unique_outports.insert(unique_outports.begin(), outport);
        else
            unique_outports.push_back(outport);
    }
    return unique_outports;
}