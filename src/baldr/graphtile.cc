#include "baldr/graphtile.h"
#include "baldr/graphtilestorage.h"
#include "baldr/datetime.h"
#include <valhalla/midgard/logging.h>

#include <ctime>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

namespace valhalla {
namespace baldr {

// Default constructor
GraphTile::GraphTile()
    : size_(0),
      header_(nullptr),
      nodes_(nullptr),
      directededges_(nullptr),
      departures_(nullptr),
      transit_stops_(nullptr),
      transit_routes_(nullptr),
      transit_schedules_(nullptr),
      access_restrictions_(nullptr),
      signs_(nullptr),
      admins_(nullptr),
      edge_bins_(nullptr),
      edgeinfo_(nullptr),
      textlist_(nullptr),
      edgeinfo_size_(0),
      textlist_size_(0){
}

// Constructor given a filename. Reads the graph data into memory.
GraphTile::GraphTile(const TileHierarchy& hierarchy, const GraphId& graphid)
    : size_(0) {

  // Don't bother with invalid ids
  if (!graphid.Is_Valid())
    return;

  std::vector<char> tile_data;
  if (hierarchy.tile_storage()->ReadTile(graphid, hierarchy, tile_data)) {
    graphtile_.reset(new char[tile_data.size()]);
    std::copy(tile_data.begin(), tile_data.end(), graphtile_.get());

    // Set a pointer to the header (first structure in the binary data).
    char* ptr = graphtile_.get();
    header_ = reinterpret_cast<GraphTileHeader*>(ptr);
    ptr += sizeof(GraphTileHeader);

    // TODO check version

    // Set a pointer to the node list
    nodes_ = reinterpret_cast<NodeInfo*>(ptr);
    ptr += header_->nodecount() * sizeof(NodeInfo);

    // Set a pointer to the directed edge list
    directededges_ = reinterpret_cast<DirectedEdge*>(ptr);
    ptr += header_->directededgecount() * sizeof(DirectedEdge);

    // Set a pointer access restriction list
    access_restrictions_ = reinterpret_cast<AccessRestriction*>(ptr);
    ptr += header_->access_restriction_count() * sizeof(AccessRestriction);

    // Set a pointer to the transit departure list
    departures_ = reinterpret_cast<TransitDeparture*>(ptr);
    ptr += header_->departurecount() * sizeof(TransitDeparture);

    // Set a pointer to the transit stop list
    transit_stops_ = reinterpret_cast<TransitStop*>(ptr);
    ptr += header_->stopcount() * sizeof(TransitStop);

    // Set a pointer to the transit route list
    transit_routes_ = reinterpret_cast<TransitRoute*>(ptr);
    ptr += header_->routecount() * sizeof(TransitRoute);

    // Set a pointer to the transit schedule list
    transit_schedules_ = reinterpret_cast<TransitSchedule*>(ptr);
    ptr += header_->schedulecount() * sizeof(TransitSchedule);

/*
LOG_INFO("Tile: " + std::to_string(graphid.tileid()) + "," + std::to_string(graphid.level()));
LOG_INFO("Departures: " + std::to_string(header_->departurecount()) +
         " Stops: " + std::to_string(header_->stopcount()) +
         " Routes: " + std::to_string(header_->routecount()) +
         " Transfers: " + std::to_string(header_->transfercount()) +
         " Exceptions: " + std::to_string(header_->calendarcount()));
    */

    // Set a pointer to the sign list
    signs_ = reinterpret_cast<Sign*>(ptr);
    ptr += header_->signcount() * sizeof(Sign);

    // Set a pointer to the admininstrative information list
    admins_ = reinterpret_cast<Admin*>(ptr);
    ptr += header_->admincount() * sizeof(Admin);

    // Set a pointer to the edge bin list
    edge_bins_ = reinterpret_cast<GraphId*>(ptr);

    // Start of edge information and its size
    edgeinfo_ = graphtile_.get() + header_->edgeinfo_offset();
    edgeinfo_size_ = header_->textlist_offset() - header_->edgeinfo_offset();

    // Start of text list and its size
    textlist_ = graphtile_.get() + header_->textlist_offset();
    textlist_size_ = tile_data.size() - header_->textlist_offset();

    //if this tile is transit then we need to save off the pair<tileid,lineid> lookup via
    //onestop_ids.  This will be used for including or excluding transit lines for transit
    //routes.  We save 2 maps because operators contain all of their route's tile_line pairs
    //and it is used to include or exclude the operator as a whole.
    if (graphid.level() == 3) {

      stop_one_stops.reserve(header_->stopcount());
      for (uint32_t i = 0; i < header_->stopcount(); i++) {
        const auto& stop = GetName(transit_stops_[i].one_stop_offset());
        stop_one_stops[stop] = tile_index_pair(graphid.tileid(),i);
      }

      auto deps = GetTransitDepartures();
      for(auto const& dep: deps) {
        const auto* t = GetTransitRoute(dep.second->routeid());
        const auto& route_one_stop = GetName(t->one_stop_offset());
        auto stops = route_one_stops.find(route_one_stop);
        if (stops == route_one_stops.end()) {
          std::list<tile_index_pair> tile_line_ids;
          tile_line_ids.emplace_back(tile_index_pair(graphid.tileid(), dep.second->lineid()));
          route_one_stops[route_one_stop] = tile_line_ids;
        } else {
          route_one_stops[route_one_stop].emplace_back(tile_index_pair(graphid.tileid(), dep.second->lineid()));
        }

        // operators contain all of their route's tile_line pairs.
        const auto& op_one_stop = GetName(t->op_by_onestop_id_offset());
        stops = oper_one_stops.find(op_one_stop);
        if (stops == oper_one_stops.end()) {
          std::list<tile_index_pair> tile_line_ids;
          tile_line_ids.emplace_back(tile_index_pair(graphid.tileid(), dep.second->lineid()));
          oper_one_stops[op_one_stop] = tile_line_ids;
        } else {
          oper_one_stops[op_one_stop].emplace_back(tile_index_pair(graphid.tileid(), dep.second->lineid()));
        }
      }
    }

    // Set the size to indicate success
    size_ = tile_data.size();
  }
  else {
    LOG_DEBUG("Tile " + file_location + " was not found");
  }
}

GraphTile::~GraphTile() {
}

// Get the bounding box of this graph tile.
AABB2<PointLL> GraphTile::BoundingBox(const TileHierarchy& hierarchy) const {

  //figure the largest id for this level
  auto level = hierarchy.levels().find(header_->graphid().level());
  if(level == hierarchy.levels().end() &&
      header_->graphid().level() == ((hierarchy.levels().rbegin())->second.level+1))
    level = hierarchy.levels().begin();

  auto tiles = level->second.tiles;
  return tiles.TileBounds(header_->graphid().tileid());
}

size_t GraphTile::size() const {
  return size_;
}

GraphId GraphTile::id() const {
  return header_->graphid();
}

const GraphTileHeader* GraphTile::header() const {
  return header_;
}

const NodeInfo* GraphTile::node(const GraphId& node) const {
  if (node.id() < header_->nodecount())
    return &nodes_[node.id()];
  throw std::runtime_error("GraphTile NodeInfo index out of bounds: " +
                             std::to_string(node.tileid()) + "," +
                             std::to_string(node.level()) + "," +
                             std::to_string(node.id()) + " nodecount= " +
                             std::to_string(header_->nodecount()));
}

const NodeInfo* GraphTile::node(const size_t idx) const {
  if (idx < header_->nodecount())
    return &nodes_[idx];
  throw std::runtime_error("GraphTile NodeInfo index out of bounds: " +
                           std::to_string(header_->graphid().tileid()) + "," +
                           std::to_string(header_->graphid().level()) + "," +
                           std::to_string(idx)  + " nodecount= " +
                           std::to_string(header_->nodecount()));
}

// Get the directed edge given a GraphId
const DirectedEdge* GraphTile::directededge(const GraphId& edge) const {
  if (edge.id() < header_->directededgecount())
    return &directededges_[edge.id()];
  throw std::runtime_error("GraphTile DirectedEdge index out of bounds: " +
                           std::to_string(header_->graphid().tileid()) + "," +
                           std::to_string(header_->graphid().level()) + "," +
                           std::to_string(edge.id())  + " directededgecount= " +
                           std::to_string(header_->directededgecount()));
}

// Get the directed edge at the specified index.
const DirectedEdge* GraphTile::directededge(const size_t idx) const {
  if (idx < header_->directededgecount())
    return &directededges_[idx];
  throw std::runtime_error("GraphTile DirectedEdge index out of bounds: " +
                           std::to_string(header_->graphid().tileid()) + "," +
                           std::to_string(header_->graphid().level()) + "," +
                           std::to_string(idx)  + " directededgecount= " +
                           std::to_string(header_->directededgecount()));
}

// Convenience method to get opposing edge Id given a directed edge.
// The end node of the directed edge must be in this tile.
GraphId GraphTile::GetOpposingEdgeId(const DirectedEdge* edge) const {
  GraphId endnode = edge->endnode();
  return { endnode.tileid(), endnode.level(),
           node(endnode.id())->edge_index() + edge->opp_index() };
}

// Get a pointer to edge info.
std::unique_ptr<const EdgeInfo> GraphTile::edgeinfo(const size_t offset) const {
  return std::unique_ptr<EdgeInfo>(new EdgeInfo(edgeinfo_ + offset, textlist_, textlist_size_));
}

// Get the directed edges outbound from the specified node index.
const DirectedEdge* GraphTile::GetDirectedEdges(const uint32_t node_index,
                                                uint32_t& count,
                                                uint32_t& edge_index) const {
  const NodeInfo* nodeinfo = node(node_index);
  count = nodeinfo->edge_count();
  edge_index = nodeinfo->edge_index();
  return directededge(nodeinfo->edge_index());
}

// Convenience method to get the names for an edge given the offset to the
// edge info
std::vector<std::string> GraphTile::GetNames(const uint32_t edgeinfo_offset) const {
  return edgeinfo(edgeinfo_offset)->GetNames();
}

// Get the admininfo at the specified index.
AdminInfo GraphTile::admininfo(const size_t idx) const {
  if (idx < header_->admincount()) {
    const Admin& admin = admins_[idx];
    return AdminInfo(textlist_ + admin.country_offset(),
                     textlist_ + admin.state_offset(),
                     admin.country_iso(), admin.state_iso());
  }
  throw std::runtime_error("GraphTile AdminInfo index out of bounds");
}

// Get the admin at the specified index.
const Admin* GraphTile::admin(const size_t idx) const {
  if (idx < header_->admincount()) {
    return &admins_[idx];
  }
  throw std::runtime_error("GraphTile Admin index out of bounds");
}

// Convenience method to get the text/name for a given offset to the textlist
std::string GraphTile::GetName(const uint32_t textlist_offset) const {

  if (textlist_offset < textlist_size_) {
    return textlist_ + textlist_offset;
  } else {
    throw std::runtime_error("GetName: offset exceeds size of text list");
  }
}

// Convenience method to get the signs for an edge given the
// directed edge index.
std::vector<SignInfo> GraphTile::GetSigns(const uint32_t idx) const {
  uint32_t count = header_->signcount();
  std::vector<SignInfo> signs;
  if (count == 0) {
    return signs;
  }

  // Binary search
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (signs_[mid].edgeindex() == idx) {
      found = true;
      break;
    }
    if (idx < signs_[mid].edgeindex() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (found) {
    // Back up while prior is equal (or at the beginning)
    while (mid > 0 && signs_[mid-1].edgeindex() == idx) {
      mid--;
    }

    // Add signs
    while (signs_[mid].edgeindex() == idx && mid < count) {
      if (signs_[mid].text_offset() < textlist_size_) {
        signs.emplace_back(signs_[mid].type(),
                (textlist_ + signs_[mid].text_offset()));
      } else {
        throw std::runtime_error("GetSigns: offset exceeds size of text list");
      }
      mid++;
    }
  }
  if (signs.size() == 0) {
    LOG_ERROR("No signs found for idx = " + std::to_string(idx));
  }
  return signs;
}

// Get the next departure given the directed line Id and the current
// time (seconds from midnight).
const TransitDeparture* GraphTile::GetNextDeparture(const uint32_t lineid,
                 const uint32_t current_time, const uint32_t day,
                 const uint32_t dow, bool date_before_tile) const {
  uint32_t count = header_->departurecount();
  if (count == 0) {
    return nullptr;
  }

  // Departures are sorted by edge Id and then by departure time.
  // Binary search to find a departure with matching line Id.
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (departures_[mid].lineid() == lineid) {
      found = true;
      break;
    }
    if (lineid < departures_[mid].lineid()) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (!found) {
    LOG_DEBUG("No departures found for lineid = " + std::to_string(lineid));
    return nullptr;
  }

  // Back up until the prior departure from this edge has departure time
  // less than the current time
  while (mid > 0 &&
         departures_[mid-1].lineid() == lineid &&
         departures_[mid-1].departure_time() >= current_time) {
    mid--;
  }

  // Iterate through departures until one is found with valid date, dow or
  // calendar date, and does not have a calendar exception.
  const TransitDeparture* dep = &departures_[mid];
  while (true) {
    // Make sure valid departure time
    if (dep->departure_time() >= current_time) {
      if (GetTransitSchedule(dep->schedule_index())->IsValid(day, dow, date_before_tile)) {
        return dep;
      }
    }

    // Advance to next departure and break if
    if (mid++ == count) {
      break;
    }
    dep++;
    if (dep->lineid() != lineid) {
      break;
    }
  }

  // TODO - maybe wrap around, try next day?
  LOG_DEBUG("No more departures found for lineid = " + std::to_string(lineid) +
           " current_time = " + std::to_string(current_time));
  return nullptr;
}

// Get the departure given the line Id and tripid
const TransitDeparture* GraphTile::GetTransitDeparture(const uint32_t lineid,
                     const uint32_t tripid) const {
  uint32_t count = header_->departurecount();
  if (count == 0) {
    return nullptr;
  }

  // Departures are sorted by edge Id and then by departure time.
  // Binary search to find a departure with matching edge Id.
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (departures_[mid].lineid() == lineid) {
      found = true;
      break;
    }
    if (lineid < departures_[mid].lineid() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (!found) {
    LOG_INFO("No departures found for lineid = " + std::to_string(lineid) +
             " and tripid = " + std::to_string(tripid));
    return nullptr;
  }

  if (found) {
    // Back up while prior is equal (or at the beginning)
    while (mid > 0 && departures_[mid-1].lineid() == lineid) {

      if (departures_[mid].tripid() == tripid)
        return &departures_[mid];

      mid--;
    }

    while (departures_[mid].tripid() != tripid && mid < count) {
      mid++;
    }

    if (departures_[mid].tripid() == tripid)
      return &departures_[mid];
  }

  LOG_INFO("No departures found for lineid = " + std::to_string(lineid) +
           " and tripid = " + std::to_string(tripid));
  return nullptr;
}

// Get a map of departures based on lineid.  No dups exist in the map.
std::unordered_map<uint32_t,TransitDeparture*> GraphTile::GetTransitDepartures() const {

  std::unordered_map<uint32_t,TransitDeparture*> deps;
  deps.reserve(header_->departurecount());

  for (uint32_t i = 0; i < header_->departurecount(); i++)
    deps.insert({departures_[i].lineid(),&departures_[i]});

  return deps;
}

// Get the stop onestops in this tile
std::unordered_map<std::string, tile_index_pair>
GraphTile::GetStopOneStops() const {
  return stop_one_stops;
}

// Get the route onestops in this tile.
std::unordered_map<std::string, std::list<tile_index_pair>>
GraphTile::GetRouteOneStops() const {
  return route_one_stops;
}

// Get the operator onestops in this tile.
std::unordered_map<std::string, std::list<tile_index_pair>>
GraphTile::GetOperatorOneStops() const {
  return oper_one_stops;
}

// Get the transit stop given its index within the tile.
const TransitStop* GraphTile::GetTransitStop(const uint32_t idx) const {
  uint32_t count = header_->stopcount();
  if (count == 0)
    return nullptr;

  if (idx < count)
    return &transit_stops_[idx];
  throw std::runtime_error("GraphTile Transit Stop index out of bounds");
}

// Get the transit route given its index within the tile.
const TransitRoute* GraphTile::GetTransitRoute(const uint32_t idx) const {
  uint32_t count = header_->routecount();
  if (count == 0)
    return nullptr;

  if (idx < count) {
    return &transit_routes_[idx];
  }
  throw std::runtime_error("GraphTile GetTransitRoute index out of bounds");
}

// Get the transit schedule given its schedule index.
const TransitSchedule* GraphTile::GetTransitSchedule(const uint32_t idx) const {
  uint32_t count = header_->schedulecount();
  if (count == 0)
    return nullptr;

  if (idx < count) {
    return &transit_schedules_[idx];
  }
  throw std::runtime_error("GraphTile GetTransitSchedule index out of bounds");
}

// Get the access restriction given its directed edge index
std::vector<AccessRestriction> GraphTile::GetAccessRestrictions(const uint32_t idx,
                                                                const uint32_t access) const {

  std::vector<AccessRestriction> restrictions;
  uint32_t count = header_->access_restriction_count();
  if (count == 0) {
    return restrictions;
  }

  // Access restriction are sorted by edge Id.
  // Binary search to find a access restriction with matching edge Id.
  int32_t low = 0;
  int32_t high = count-1;
  int32_t mid;
  bool found = false;
  while (low <= high) {
    mid = (low + high) / 2;
    if (access_restrictions_[mid].edgeindex() == idx) {
      found = true;
      break;
    }
    if (idx < access_restrictions_[mid].edgeindex() ) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  if (!found) {
    LOG_ERROR("No restrictions found for edge index = " + std::to_string(idx));
    return restrictions;
  }

  // Back up while prior is equal (or at the beginning)
  while (mid > 0 && access_restrictions_[mid - 1].edgeindex() == idx) {
    mid--;
  }

  while (access_restrictions_[mid].edgeindex() == idx && mid < count) {
    // Add restrictions for only the access that we are interested in
    if (access_restrictions_[mid].modes() & access)
      restrictions.emplace_back(access_restrictions_[mid]);
    mid++;
  }

  if (restrictions.size() == 0) {
    LOG_ERROR("No restrictions found for edge index = " + std::to_string(idx));
  }
  return restrictions;
}

// Get the array of graphids for this bin
midgard::iterable_t<GraphId> GraphTile::GetBin(size_t column, size_t row) const {
  auto offsets = header_->bin_offset(column, row);
  return iterable_t<GraphId>{edge_bins_ + offsets.first, edge_bins_ + offsets.second};
}

midgard::iterable_t<GraphId> GraphTile::GetBin(size_t index) const {
  auto offsets = header_->bin_offset(index);
  return iterable_t<GraphId>{edge_bins_ + offsets.first, edge_bins_ + offsets.second};
}

}
}
