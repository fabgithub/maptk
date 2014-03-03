/*ckwg +5
 * Copyright 2014 by Kitware, Inc. All Rights Reserved. Please refer to
 * KITWARE_LICENSE.TXT for licensing information, or contact General Counsel,
 * Kitware, Inc., 28 Corporate Drive, Clifton Park, NY 12065.
 */

/**
 * \file
 * \brief Implementation of VXL triangulate landmarks algorithm
 */

#include <maptk/vxl/triangulate_landmarks.h>
#include <maptk/vxl/camera_map.h>
#include <boost/foreach.hpp>
#include <set>
#include <vpgl/algo/vpgl_triangulate_points.h>

namespace maptk
{

namespace vxl
{


/// Private implementation class
class triangulate_landmarks::priv
{
public:
  /// Constructor
  priv()
  {
  }

  priv(const priv& other)
  {
  }

  /// parameters - none yet
};


/// Constructor
triangulate_landmarks
::triangulate_landmarks()
: d_(new priv)
{
}


/// Copy Constructor
triangulate_landmarks
::triangulate_landmarks(const triangulate_landmarks& other)
: d_(new priv(*other.d_))
{
}


/// Destructor
triangulate_landmarks
::~triangulate_landmarks()
{
}


/// Get this algorithm's \link maptk::config_block configuration block \endlink
config_block_sptr
triangulate_landmarks
::get_configuration() const
{
  // get base config from base class
  config_block_sptr config = maptk::algo::triangulate_landmarks::get_configuration();
  return config;
}


/// Set this algorithm's properties via a config block
void
triangulate_landmarks
::set_configuration(config_block_sptr in_config)
{
}


/// Check that the algorithm's currently configuration is valid
bool
triangulate_landmarks
::check_configuration(config_block_sptr config) const
{
  return true;
}


/// Triangulate the landmark locations given sets of cameras and tracks
void
triangulate_landmarks
::triangulate(camera_map_sptr cameras,
              track_set_sptr tracks,
              landmark_map_sptr& landmarks) const
{
  if( !cameras || !landmarks || !tracks )
  {
    // TODO throw an exception for missing input data
    return;
  }
  typedef vxl::camera_map::map_vcam_t map_vcam_t;
  typedef maptk::landmark_map::map_landmark_t map_landmark_t;

  // extract data from containers
  map_vcam_t vcams = camera_map_to_vpgl(*cameras);
  map_landmark_t lms = landmarks->landmarks();
  std::vector<track_sptr> trks = tracks->tracks();

  // build a track map by id
  typedef std::map<track_id_t, track_sptr> track_map_t;
  track_map_t track_map;
  BOOST_FOREACH(const track_sptr& t, trks)
  {
    track_map[t->id()] = t;
  }

  map_landmark_t triangulated_lms;
  BOOST_FOREACH(const map_landmark_t::value_type& p, lms)
  {
    // get the corresponding track
    track_map_t::const_iterator t_itr = track_map.find(p.first);
    if (t_itr == track_map.end())
    {
      // there is no track for the provided landmark
      continue;
    }
    const track& t = *t_itr->second;

    // extract the cameras and image points for this landmarks
    std::vector<vpgl_perspective_camera<double> > lm_cams;
    std::vector<vgl_point_2d<double> > lm_image_pts;

    for (track::history_const_itr tsi = t.begin(); tsi != t.end(); ++tsi)
    {
      if (!tsi->feat)
      {
        // there is no valid feature for this track state
        continue;
      }
      map_vcam_t::const_iterator c_itr = vcams.find(tsi->frame_id);
      if (c_itr == vcams.end())
      {
        // there is no camera for this track state.
        continue;
      }
      lm_cams.push_back(c_itr->second);
      vector_2d pt = tsi->feat->loc();
      lm_image_pts.push_back(vgl_point_2d<double>(pt.x(), pt.y()));
    }

    // if we found at least two views of this landmark, triangulate
    if (lm_cams.size() > 1)
    {
      vector_3d lm_loc = p.second->loc();
      vgl_point_3d<double> pt3d(lm_loc.x(), lm_loc.y(), lm_loc.z());
      double error = vpgl_triangulate_points::triangulate(lm_image_pts,
                                                          lm_cams, pt3d);
      landmark_d* lm = new landmark_d(vector_3d(pt3d.x(), pt3d.y(), pt3d.z()));
      lm->set_covar(covariance_3d(error));
      triangulated_lms[p.first] = landmark_sptr(lm);
    }
  }
  landmarks = landmark_map_sptr(new simple_landmark_map(triangulated_lms));
}


} // end namespace vxl

} // end namespace maptk