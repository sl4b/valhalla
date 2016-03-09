#include "midgard/tiles.h"
#include "midgard/polyline2.h"
#include "midgard/util.h"
#include "midgard/distanceapproximator.h"
#include <cmath>
#include <set>
#include <iostream>

namespace {

  //this is modified to include all pixels that are intersected by the floating point line
  //at each step it decides to either move in the x or y direction based on which pixels midpoint
  //forms a smaller triangle with the line. to avoid edge cases we allow set_pixel to make the
  //the loop bail if we leave the valid drawing region
  void bresenham_line(float x0, float y0, float x1, float y1, const std::function<bool (int32_t, int32_t)>& set_pixel) {
    //this one for sure
    bool outside = set_pixel(std::floor(x0), std::floor(y0));
    //steps in the proper direction and constants for shoelace formula
    float sx = x0 < x1 ? 1 : -1, dx = x1 - x0, x = std::floor(x0) + .5f;
    float sy = y0 < y1 ? 1 : -1, dy = y1 - y0, y = std::floor(y0) + .5f;
    //keep going until we make it to the ending pixel
    while(std::floor(x) != std::floor(x1) || std::floor(y) != std::floor(y1)) {
      float tx = std::abs(dx*(y - y0) - dy*((x + sx) - x0));
      float ty = std::abs(dx*((y + sy) - y0) - dy*(x - x0));
      //less error moving in the x
      if(tx < ty) { x += sx; }
      //less error moving in the y
      else { y += sy; }
      //mark this pixel
      bool o = set_pixel(std::floor(x), std::floor(y));
      if(outside == false && o == true)
        return;
      outside = o;
    }
  }

}

namespace valhalla {
namespace midgard {

// Constructor.  A bounding box and tile size is specified.
// Sets class data members and computes the number of rows and columns
// based on the bounding box and tile size.
template <class coord_t>
Tiles<coord_t>::Tiles(const AABB2<coord_t>& bounds, const float tilesize, unsigned short subdivisions):
  tilebounds_(bounds), tilesize_(tilesize), nsubdivisions_(subdivisions){
  tilebounds_ = bounds;
  tilesize_ = tilesize;
  subdivision_size_ = tilesize_ / nsubdivisions_;
  ncolumns_ = static_cast<int32_t>(ceil((bounds.maxx() - bounds.minx()) / tilesize_));
  nrows_    = static_cast<int32_t>(ceil((bounds.maxy() - bounds.miny()) / tilesize_));
}

// Get the tile size. Tiles are square.
template <class coord_t>
float Tiles<coord_t>::TileSize() const {
  return tilesize_;
}

template <class coord_t>
float Tiles<coord_t>::SubdivisionSize() const {
  return subdivision_size_;
}

// Get the bounding box of the tiling system.
template <class coord_t>
AABB2<coord_t> Tiles<coord_t>::TileBounds() const {
  return tilebounds_;
}

// Get the number of rows in the tiling system.
template <class coord_t>
int32_t Tiles<coord_t>::nrows() const {
  return nrows_;
}

// Get the number of columns in the tiling system.
template <class coord_t>
int32_t Tiles<coord_t>::ncolumns() const {
  return ncolumns_;
}

template <class coord_t>
unsigned short Tiles<coord_t>::nsubdivisions() const {
  return nsubdivisions_;
}

// Get the "row" based on y.
template <class coord_t>
int32_t Tiles<coord_t>::Row(const float y) const {
  // Return -1 if outside the tile system bounds
  if (y < tilebounds_.miny() || y > tilebounds_.maxy())
    return -1;

  // If equal to the max y return the largest row
  if (y == tilebounds_.maxy())
    return nrows_ - 1;
  else {
    return static_cast<int32_t>((y - tilebounds_.miny()) / tilesize_);
  }
}

// Get the "column" based on x.
template <class coord_t>
int32_t Tiles<coord_t>::Col(const float x) const {
  // Return -1 if outside the tile system bounds
  if (x < tilebounds_.minx() || x > tilebounds_.maxx())
    return -1;

  // If equal to the max x return the largest column
  if (x == tilebounds_.maxx())
    return ncolumns_ - 1;
  else {
    float col = (x - tilebounds_.minx()) / tilesize_;
    return (col >= 0.0) ? static_cast<int32_t>(col) :
                          static_cast<int32_t>(col - 1);
  }
}

// Convert a coordinate into a tile Id. The point is within the tile.
template <class coord_t>
int32_t Tiles<coord_t>::TileId(const coord_t& c) const {
  return TileId(c.y(), c.x());
}

// Convert x,y to a tile Id.
template <class coord_t>
int32_t Tiles<coord_t>::TileId(const float y, const float x) const {
  // Return -1 if totally outside the extent.
  if (y < tilebounds_.miny() || x < tilebounds_.minx() ||
      y > tilebounds_.maxy() || x > tilebounds_.maxx())
    return -1;

  // Find the tileid by finding the latitude row and longitude column
  return (Row(y) * ncolumns_) + Col(x);
}

// Get the tile Id given the row Id and column Id.
template <class coord_t>
int32_t Tiles<coord_t>::TileId(const int32_t col, const int32_t row) const {
  return (row * ncolumns_) + col;
}

// Get the tile row, col based on tile Id.
template <class coord_t>
std::pair<int32_t, int32_t> Tiles<coord_t>::GetRowColumn(
                        const int32_t tileid) const {
  return { tileid / ncolumns_, tileid % ncolumns_ };
}

// Get a maximum tileid given a bounds and a tile size.
template <class coord_t>
uint32_t Tiles<coord_t>::MaxTileId(const AABB2<coord_t>& bbox,
                                   const float tile_size) {
  uint32_t cols = static_cast<uint32_t>(std::ceil(bbox.Width() / tile_size));
  uint32_t rows = static_cast<uint32_t>(std::ceil(bbox.Height() / tile_size));
  return (cols * rows) - 1;
}

// Get the base x,y (or lng,lat) of a specified tile.
template <class coord_t>
coord_t Tiles<coord_t>::Base(const int32_t tileid) const {
  int32_t row = tileid / ncolumns_;
  int32_t col = tileid - (row * ncolumns_);
  return coord_t(tilebounds_.minx() + (col * tilesize_),
                 tilebounds_.miny() + (row * tilesize_));
}

// Get the bounding box of the specified tile.
template <class coord_t>
AABB2<coord_t> Tiles<coord_t>::TileBounds(const int32_t tileid) const {
  Point2 base = Base(tileid);
  return AABB2<coord_t>(base.x(), base.y(),
                        base.x() + tilesize_, base.y() + tilesize_);
}

// Get the bounding box of the tile with specified row, column.
template <class coord_t>
AABB2<coord_t> Tiles<coord_t>::TileBounds(const int32_t col,
                                          const int32_t row) const {
  float basex = tilebounds_.minx() + ((float) col * tilesize_);
  float basey = tilebounds_.miny() + ((float) row * tilesize_);
  return AABB2<coord_t>(basex, basey, basex + tilesize_, basey + tilesize_);
}

// Get the center of the specified tile.
template <class coord_t>
coord_t Tiles<coord_t>::Center(const int32_t tileid) const {
  Point2 base = Base(tileid);
  return coord_t(base.x() + tilesize_ * 0.5, base.y() + tilesize_ * 0.5);
}

// Get the tile Id given a previous tile and a row, column offset.
template <class coord_t>
int32_t Tiles<coord_t>::GetRelativeTileId(const int32_t initial_tile,
                             const int32_t delta_rows,
                             const int32_t delta_cols) const {
  return initial_tile + (delta_rows * ncolumns_) + delta_cols;
}

// Get the tile offsets (row,column) between the previous tile Id and
// a new tileid.  The offsets are returned through arguments (references).
// Offsets can be positive or negative or 0.
template <class coord_t>
void Tiles<coord_t>::TileOffsets(const int32_t initial_tileid, const int32_t newtileid,
                        int& delta_rows, int& delta_cols) const {
  int32_t deltaTile = newtileid - initial_tileid;
  delta_rows = (newtileid / ncolumns_) - (initial_tileid / ncolumns_);
  delta_cols = deltaTile - (delta_rows * ncolumns_);
}

// Get the number of tiles in the tiling system.
template <class coord_t>
uint32_t Tiles<coord_t>::TileCount() const {
  float nrows = (tilebounds_.maxy() - tilebounds_.miny()) / tilesize_;
  return ncolumns_ * static_cast<int32_t>(ceil(nrows));
}

// Get the neighboring tileid to the right/east.
template <class coord_t>
int32_t Tiles<coord_t>::RightNeighbor(const int32_t tileid) const {
  int32_t row = tileid / ncolumns_;
  int32_t col = tileid - (row * ncolumns_);
  return (col < ncolumns_ - 1) ? tileid + 1 : tileid - ncolumns_ + 1;
}

// Get the neighboring tileid to the left/west.
template <class coord_t>
int32_t Tiles<coord_t>::LeftNeighbor(const int32_t tileid) const {
  int32_t row = tileid / ncolumns_;
  int32_t col = tileid - (row * ncolumns_);
  return (col > 0) ? tileid - 1 : tileid + ncolumns_ - 1;
}

// Get the neighboring tileid above or north.
template <class coord_t>
int32_t Tiles<coord_t>::TopNeighbor(const int32_t tileid) const {
  return (tileid < static_cast<int32_t>((TileCount() - ncolumns_))) ?
              tileid + ncolumns_ : tileid;
}

// Get the neighboring tileid below or south.
template <class coord_t>
int32_t Tiles<coord_t>::BottomNeighbor(const int32_t tileid) const {
  return (tileid < ncolumns_) ? tileid : tileid - ncolumns_;
}

// Checks if 2 tiles are neighbors (N,E,S,W).
template <class coord_t>
bool Tiles<coord_t>::AreNeighbors(const uint32_t id1, const uint32_t id2) const {
  return (id2 == TopNeighbor(id1) ||
          id2 == RightNeighbor(id1) ||
          id2 == BottomNeighbor(id1) ||
          id2 == LeftNeighbor(id1));
}

// Get the list of tiles that lie within the specified bounding box.
// The method finds the center tile and spirals out by finding neighbors
// and recursively checking if tile is inside and checking/adding
// neighboring tiles
template <class coord_t>
std::vector<int> Tiles<coord_t>::TileList(const AABB2<coord_t>& bbox) const {
  // Get tile at the center of the bounding box. Return -1 if the center
  // of the bounding box is not within the tiling system bounding box.
  // TODO - relax this to check edges of the bounding box?
  std::vector<int32_t> tilelist;
  int32_t tileid = TileId(bbox.Center());
  if (tileid == -1)
    return tilelist;

  // List of tiles to check if in view. Use a list: push new entries on the
  // back and pop off the front. The tile search tends to spiral out from
  // the center.
  std::list<int32_t> checklist;

  // Visited tiles
  std::unordered_set<int32_t> visited_tiles;

  // Set this tile in the checklist and it to the list of visited tiles.
  checklist.push_back(tileid);
  visited_tiles.insert(tileid);

  // Get neighboring tiles in bounding box until NextTile returns -1
  // or the maximum number specified is reached
  while (!checklist.empty()) {
    // Get the element off the front of the list and add it to the tile list.
    tileid = checklist.front();
    checklist.pop_front();
    tilelist.push_back(tileid);

    // Check neighbors
    int32_t neighbor = LeftNeighbor(tileid);
    if (visited_tiles.find(neighbor) == visited_tiles.end() &&
        bbox.Intersects(TileBounds(neighbor))) {
      checklist.push_back(neighbor);
      visited_tiles.insert(neighbor);
    }
    neighbor = RightNeighbor(tileid);
    if (visited_tiles.find(neighbor) == visited_tiles.end() &&
        bbox.Intersects(TileBounds(neighbor))) {
      checklist.push_back(neighbor);
      visited_tiles.insert(neighbor);
    }
    neighbor = TopNeighbor(tileid);
    if (visited_tiles.find(neighbor) == visited_tiles.end() &&
        bbox.Intersects(TileBounds(neighbor))) {
      checklist.push_back(neighbor);
      visited_tiles.insert(neighbor);
    }
    neighbor = BottomNeighbor(tileid);
    if (visited_tiles.find(neighbor) == visited_tiles.end() &&
        bbox.Intersects(TileBounds(neighbor))) {
      checklist.push_back(neighbor);
      visited_tiles.insert(neighbor);
    }
  }
  return tilelist;
}

// Color a "connectivity map" starting with a sparse map of uncolored tiles.
// Any 2 tiles that have a connected path between them will have the same
// value in the connectivity map.
template <class coord_t>
void Tiles<coord_t>::ColorMap(std::unordered_map<uint32_t,
                              size_t>& connectivity_map) const {
  // Connectivity map - all connected regions will have a unique Id. If any 2
  // tile Ids have a different Id they are judged to be not-connected.

  // Iterate through tiles
  size_t color = 1;
  for (auto& tile : connectivity_map) {
    // Continue if already visited
    if (tile.second > 0) {
      continue;
    }

    // Mark this tile Id with the current color and find all its
    // accessible neighbors
    tile.second = color;
    std::list<uint32_t> checklist{tile.first};
    while (!checklist.empty()) {
      uint32_t next_tile = checklist.front();
      checklist.pop_front();

      // Check neighbors.
      uint32_t neighbor = LeftNeighbor(next_tile);
      auto neighbor_itr = connectivity_map.find(neighbor);
      if (neighbor_itr != connectivity_map.cend() && neighbor_itr->second == 0) {
        checklist.push_back(neighbor);
        neighbor_itr->second = color;
      }
      neighbor = RightNeighbor(next_tile);
      neighbor_itr = connectivity_map.find(neighbor);
      if (neighbor_itr != connectivity_map.cend() && neighbor_itr->second == 0) {
        checklist.push_back(neighbor);
        neighbor_itr->second = color;
      }
      neighbor = TopNeighbor(next_tile);
      neighbor_itr = connectivity_map.find(neighbor);
      if (neighbor_itr != connectivity_map.cend() && neighbor_itr->second == 0) {
        checklist.push_back(neighbor);
        neighbor_itr->second = color;
      }
      neighbor = BottomNeighbor(next_tile);
      neighbor_itr = connectivity_map.find(neighbor);
      if (neighbor_itr != connectivity_map.cend() && neighbor_itr->second == 0) {
        checklist.push_back(neighbor);
        neighbor_itr->second = color;
      }
    }

    // Increment color
    color++;
  }
}

template <class coord_t>
template <class container_t>
std::unordered_map<int32_t, std::unordered_set<unsigned short> > Tiles<coord_t>::Intersect(const container_t& linestring) const {
  std::unordered_map<int32_t, std::unordered_set<unsigned short> > intersection;

  //what to do when we want to mark a subdivision as containing a segment of this linestring
  const auto set_pixel = [this, &intersection](int32_t x, int32_t y) {
    //cant mark ones that are outside the valid range of tiles
    //TODO: wrap coordinates around x and y?
    if(x < 0 || y < 0 || x >= nsubdivisions_ * ncolumns_ || y >= nsubdivisions_ * nrows_)
      return true;
    //find the tile
    int32_t tile_column = x / nsubdivisions_;
    int32_t tile_row = y / nsubdivisions_;
    int32_t tile = tile_row * ncolumns_ + tile_column;
    //find the subdivision
    unsigned short subdivision = (y % nsubdivisions_) * nsubdivisions_ + (x % nsubdivisions_);
    intersection[tile].insert(subdivision);
    return false;
  };

  //if coord_t is spherical and the segment uv is sufficiently long then the geodesic along it
  //cannot be approximated with linear constructs so instead we resample it at a sufficiently
  //small interval so as to approximate the arc with piecewise linear segments
  container_t resampled;
  auto max_meters = subdivision_size_ * .25f * DistanceApproximator::MetersPerLngDegree(linestring.front().second);
  if(coord_t::IsSpherical() && Polyline2<coord_t>::Length(linestring) > max_meters)
    resampled = resample_spherical_polyline(linestring, max_meters, true);

  //for each segment
  const auto& line = resampled.size() ? resampled : linestring;
  auto ui = line.cbegin(), vi = line.cbegin();
  while(vi != line.cend()) {
    //figure out what the segment is
    auto u = *ui;
    auto v = u;
    std::advance(vi, 1);
    if(vi != line.cend())
      v = *vi;
    else if(line.size() > 1)
      return intersection;
    ui = vi;

    //figure out global subdivision start and end points
    auto x0 = (u.first - tilebounds_.minx()) / tilebounds_.Width() * ncolumns_ * nsubdivisions_;
    auto y0 = (u.second - tilebounds_.miny()) / tilebounds_.Height() * nrows_ * nsubdivisions_;
    auto x1 = (v.first - tilebounds_.minx()) / tilebounds_.Width() * ncolumns_ * nsubdivisions_;
    auto y1 = (v.second - tilebounds_.miny()) / tilebounds_.Height() * nrows_ * nsubdivisions_;

    //its likely for our use case that its all in one cell
    if(static_cast<int>(x0) == static_cast<int>(x1) && static_cast<int>(y0) == static_cast<int>(y1))
      set_pixel(std::floor(x0), std::floor(y0));
    //pretend the subdivisions are pixels and we are doing line rasterization
    else
      bresenham_line(x0, y0, x1, y1, set_pixel);
  }

  //give them back
  return intersection;
}

template <class coord_t>
std::function<std::tuple<int32_t, unsigned short, float>() > Tiles<coord_t>::ClosestFirst(const coord_t& seed) const {
  //what global subdivision are we starting in
  //TODO: worry about wrapping around valid range
  auto x = (seed.first - tilebounds_.minx()) / tilebounds_.Width() * ncolumns_ * nsubdivisions_;
  auto y = (seed.second - tilebounds_.miny()) / tilebounds_.Height() * nrows_ * nsubdivisions_;
  auto subdivision = static_cast<int32_t>(y * nsubdivisions_) + static_cast<int32_t>(x);
  //we could use a map here but we want a deterministic sort for testing purposes
  using best_t = std::pair<float, int32_t>;
  std::set<best_t, std::function<bool (const best_t&, const best_t&) > > queue(
    [](const best_t&a, const best_t& b){
      return a.first == b.first ? a.second < b.second : a.first < b.first;
    }
  );
  std::unordered_set<int32_t> queued {subdivision};
  //something to measure the closest possible point of a subdivision from the given seed point
  auto dist = [this, &x, &y, &seed](int32_t sub) -> float {
    auto sx = sub % (ncolumns_ * nsubdivisions_); if (sx < x) ++sx;
    auto sy = sub / (nrows_ * nsubdivisions_); if (sy < y) ++sy;
    coord_t c(sx * subdivision_size_, sy * subdivision_size_);
    //if its purely vertical then dont use a corner
    if(sx > x && sx - 1 < x)
      c.first = seed.first;
    //if its purely horizontal then dont use a corner
    if(sy > y && sy - 1 < y)
      c.second = seed.second;
    return seed.DistanceSquared(c);
  };
  //expand one at a time, always the next closest subdivision first
  return [this, &x, &y, &dist, queue, queued]() mutable {
    //get the next closest one or bail
    if(!queue.size())
      throw std::runtime_error("Subdivisions were exhausted");
    auto best = *queue.cbegin();
    queue.erase(queue.cbegin());
    //add its neighbors
    auto left = (best.second - 1) % nsubdivisions_;
    auto right = (best.second + 1) % nsubdivisions_;
    auto up = best.second < nsubdivisions_ * (nsubdivisions_ - 1) ?
                best.second + nsubdivisions_ :
                nsubdivisions_ % (best.second + nsubdivisions_ / 2);
    auto down = best.second < nsubdivisions_ ?
                nsubdivisions_ * (nsubdivisions_ - 1) + (nsubdivisions_ % (best.second + nsubdivisions_ / 2)) :
                best.second - nsubdivisions_;
    if(queued.find(left) == queued.cend()) { queued.emplace(left); queue.emplace(std::make_pair(dist(left),left)); }
    if(queued.find(right) == queued.cend()) { queued.emplace(right); queue.emplace(std::make_pair(dist(right),right)); }
    if(queued.find(up) == queued.cend()) { queued.emplace(up); queue.emplace(std::make_pair(dist(up),up)); }
    if(queued.find(down) == queued.cend()) { queued.emplace(down); queue.emplace(std::make_pair(dist(down),down)); }
    //return it
    auto sx = best.second % nsubdivisions_;
    auto sy = best.second / nsubdivisions_;
    auto tile_column = sx / nsubdivisions_;
    auto tile_row = sy / nsubdivisions_;
    auto tile = tile_row * ncolumns_ + tile_column;
    unsigned short subdivision = (sy % nsubdivisions_) * nsubdivisions_ + (sx % nsubdivisions_);
    return std::make_tuple(tile, subdivision, best.first);
  };
}

// Explicit instantiation
template class Tiles<Point2>;
template class Tiles<PointLL>;

template class std::unordered_map<int32_t, std::unordered_set<unsigned short> > Tiles<Point2>::Intersect(const std::list<Point2>&) const;
template class std::unordered_map<int32_t, std::unordered_set<unsigned short> > Tiles<PointLL>::Intersect(const std::list<PointLL>&) const;
template class std::unordered_map<int32_t, std::unordered_set<unsigned short> > Tiles<Point2>::Intersect(const std::vector<Point2>&) const;
template class std::unordered_map<int32_t, std::unordered_set<unsigned short> > Tiles<PointLL>::Intersect(const std::vector<PointLL>&) const;

}
}
