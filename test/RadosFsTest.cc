/*
 * Rados Filesystem - A filesystem library based in librados
 *
 * Copyright (C) 2014 CERN, Switzerland
 *
 * Author: Joaquim Rocha <joaquim.rocha@cern.ch>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License at http://www.gnu.org/licenses/lgpl-3.0.txt
 * for more details.
 */

#include <fcntl.h>
#include <cstdio>
#include <errno.h>
#include <stdexcept>

#include "RadosFsTest.hh"
#include "radosfscommon.h"

#define TEST_POOL "test-pool"

RadosFsTest::RadosFsTest()
  : mConf(getenv(CONF_ENV_VAR))
{
  if (mConf == 0)
    throw std::invalid_argument("Please specify the " CONF_ENV_VAR
                                "environment variable or use the --conf=... "
                                "argument.");

  rados_create(&mCluster, 0);

  if (rados_conf_read_file(mCluster, mConf) != 0)
    throw std::invalid_argument("Problem reading configuration file.");

  rados_connect(mCluster);

  rados_pool_create(mCluster, TEST_POOL);

  rados_shutdown(mCluster);

  radosFs.init("", mConf);
}

RadosFsTest::~RadosFsTest()
{
  rados_create(&mCluster, 0);

  rados_conf_read_file(mCluster, mConf);
  rados_connect(mCluster);

  rados_pool_delete(mCluster, TEST_POOL);

  rados_shutdown(mCluster);
}

void
RadosFsTest::SetUp()
{
}

void
RadosFsTest::TearDown()
{
}

void
RadosFsTest::AddPool()
{
  int ret = radosFs.addPool(TEST_POOL, "/", 1000);

  EXPECT_EQ(0, ret);

  EXPECT_EQ(1, radosFs.pools().size());
}

void
RadosFsTest::testXAttrInFsInfo(radosfs::RadosFsInfo &info)
{
  // Get the permissions xattr by a unauthorized user

  radosFs.setIds(TEST_UID, TEST_GID);

  std::string xAttrValue;
  EXPECT_EQ(-EACCES, info.getXAttr(XATTR_PERMISSIONS, xAttrValue,
                                  XATTR_PERMISSIONS_LENGTH));

  // Get an invalid xattr

  EXPECT_EQ(-EINVAL, info.getXAttr("invalid", xAttrValue,
                                  XATTR_PERMISSIONS_LENGTH));

  // Get an inexistent xattr

  EXPECT_LT(info.getXAttr("usr.inexistent", xAttrValue,
                         XATTR_PERMISSIONS_LENGTH), 0);

  // Set a user attribute

  const std::string attr("usr.attr");
  const std::string value("value");
  EXPECT_EQ(0, info.setXAttr(attr, value));

  // Get the attribute set above

  EXPECT_EQ(value.length(), info.getXAttr(attr, xAttrValue, value.length()));

  // Check the attribtue's value

  EXPECT_EQ(value, xAttrValue);

  // Change to another user

  radosFs.setIds(TEST_UID + 1, TEST_GID + 1);

  // Set an xattr by an unauthorized user

  EXPECT_EQ(-EACCES, info.setXAttr(attr, value));

  // Get an xattr by a user who can only read

  EXPECT_EQ(value.length(), info.getXAttr(attr, xAttrValue, value.length()));

  // Check the attribute's value

  EXPECT_EQ(value, xAttrValue);

  // Remove an xattr by an unauthorized user

  EXPECT_EQ(-EACCES, info.removeXAttr(attr));

  // Get the xattrs map

  std::map<std::string, std::string> map;

  EXPECT_EQ(0, info.getXAttrsMap(map));

  // Check the xattrs map's size

  EXPECT_EQ(1, map.size());

  // Switch to the root user

  radosFs.setIds(ROOT_UID, ROOT_UID);

  map.clear();

  // Set an xattr -- when being root -- in a different user's file

  EXPECT_EQ(0, info.setXAttr("sys.attribute", "check"));

  // Get the xattrs map

  EXPECT_EQ(0, info.getXAttrsMap(map));

  // Check the xattrs map's size

  EXPECT_EQ(3, map.size());

  // Check the xattrs map's value

  EXPECT_EQ(map[attr], value);

  // Check that a sys xattr is present

  EXPECT_EQ(1, map.count(XATTR_PERMISSIONS));
}