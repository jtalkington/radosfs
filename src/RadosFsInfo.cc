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

#include <stdexcept>
#include <sys/stat.h>

#include "radosfsdefines.h"
#include "radosfscommon.h"
#include "RadosFsInfo.hh"
#include "RadosFsInfoPriv.hh"
#include "RadosFsDir.hh"
#include "RadosFsFile.hh"
#include "RadosFsPriv.hh"

RADOS_FS_BEGIN_NAMESPACE

RadosFsInfoPriv::RadosFsInfoPriv(RadosFs *radosFs, const std::string &objPath)
  : radosFs(radosFs),
    target(""),
    exists(false)
{
  setPath(objPath);
}

RadosFsInfoPriv::~RadosFsInfoPriv()
{}

int
RadosFsInfoPriv::makeRealPath(std::string &path)
{
  std::string parent = getParentDir(path, 0);
  RadosFsStat stat;

  parentDirStat.reset();

  while (parent != "")
  {
    int ret = radosFs->mPriv->stat(parent, &stat);

    if (ret == -ENOENT)
      parent = getParentDir(parent, 0);
    else if (ret == 0)
      break;
    else
      return ret;
  }

  if (parent == "")
    return -ENODEV;

  if (S_ISLNK(stat.statBuff.st_mode))
  {
    path.erase(0, parent.length());
    path = stat.translatedPath + path;

    return -EAGAIN;
  }

  if (S_ISREG(stat.statBuff.st_mode))
  {
    radosfs_debug("Problem with part of the path, it is a file: %s",
                  parent.c_str());
    return -ENOTDIR;
  }

  parentDirStat = stat;

  return 0;
}

void
RadosFsInfoPriv::setPath(const std::string &path)
{
  int ret;
  this->path = sanitizePath(path);

  stat.reset();

  if (this->path.length() > MAXIMUM_PATH_LENGTH)
  {
    this->path = PATH_SEP;
    throw std::invalid_argument("Path length is too big.");
  }

  while ((ret = makeRealPath(this->path)) == -EAGAIN)
  {}

  radosFs->mPriv->stat(this->path, &stat);
}

int
RadosFsInfoPriv::makeLink(std::string &linkPath)
{
  int ret;

  while ((ret = makeRealPath(linkPath)) == -EAGAIN)
  {}

  if (linkPath.length() > MAXIMUM_PATH_LENGTH)
  {
    radosfs_debug("Error: The link path is too big.");
    return -ENAMETOOLONG;
  }

  const RadosFsPoolSP pool =
      radosFs->mPriv->getMetadataPoolFromPath(linkPath);

  if (!pool)
    return -ENODEV;

  if (ret != 0)
  {
    radosfs_debug("Error getting the real path for link %s",
                  linkPath.c_str());
    return ret;
  }

  struct stat buff;
  std::string linkParent = getParentDir(linkPath, 0);

  if (radosFs->stat(linkParent, &buff) != 0)
  {
    radosfs_debug("Cannot create a link in a directory that doesn't exist");
    return -ENOENT;
  }

  uid_t uid;
  gid_t gid;

  radosFs->getIds(&uid, &gid);

  if (!statBuffHasPermission(buff, uid, gid, O_WRONLY))
  {
    radosfs_debug("No permissions to write in %s", linkParent.c_str());
    return -EACCES;
  }

  std::string alternativeName;

  if (S_ISDIR(stat.statBuff.st_mode))
  {
    linkPath = getDirPath(linkPath.c_str());
    alternativeName = linkPath;
    alternativeName.erase(alternativeName.length() - 1, 1);
  }
  else
  {
    if (linkPath[linkPath.length()] == PATH_SEP)
      linkPath.erase(linkPath.length() - 1, 1);

    alternativeName = linkPath;
    alternativeName += PATH_SEP;
  }

  if (radosFs->stat(alternativeName, &buff) == 0)
  {
    radosfs_debug("That path already exists: %s",
                  alternativeName.c_str());
    return -EEXIST;
  }

  if (radosFs->stat(linkPath, &buff) == 0)
  {
    radosfs_debug("The link's path already exists");
    return -EEXIST;
  }

  if (ret != 0)
  {
    radosfs_debug("Failed to retrieve the ioctx for %s", linkPath.c_str());
    return ret;
  }

  RadosFsStat linkStat = stat;
  linkStat.path = linkPath;
  linkStat.pool = pool;
  linkStat.translatedPath = this->path;
  linkStat.statBuff.st_uid = uid;
  linkStat.statBuff.st_gid = gid;
  linkStat.statBuff.st_mode = DEFAULT_MODE_LINK;

  return indexObject(&parentDirStat, &linkStat, '+');
}

RadosFsInfo::RadosFsInfo(RadosFs *radosFs, const std::string &path)
  : mPriv(new RadosFsInfoPriv(radosFs, path))
{
  update();
}

RadosFsInfo::~RadosFsInfo()
{}

RadosFsInfo::RadosFsInfo(const RadosFsInfo &otherInfo)
  : mPriv(new RadosFsInfoPriv(otherInfo.filesystem(), otherInfo.path()))
{
  update();
}

std::string
RadosFsInfo::path() const
{
  return mPriv->path;
}

void
RadosFsInfo::setPath(const std::string &path)
{
  mPriv->setPath(path);
  update();
}

RadosFs *
RadosFsInfo::filesystem() const
{
  return mPriv->radosFs;
}

void
RadosFsInfo::setFilesystem(RadosFs *radosFs)
{
  mPriv->radosFs = radosFs;
}

bool
RadosFsInfo::isFile() const
{
  return !isDir();
}

bool
RadosFsInfo::isDir() const
{
  if (!exists())
    return isDirPath(mPriv->path);

  if (isLink())
    return isDirPath(mPriv->target);

  return S_ISDIR(mPriv->stat.statBuff.st_mode);
}

bool
RadosFsInfo::exists() const
{
  return mPriv->exists;
}

int
RadosFsInfo::stat(struct stat *buff)
{
  RadosFsInfo::update();

  if (!isReadable())
    return -EPERM;

  *buff = mPriv->stat.statBuff;

  return 0;
}

void
RadosFsInfo::update()
{
  mPriv->exists = false;

  if (mPriv->target != "")
  {
    mPriv->target = "";
  }

  mPriv->exists = mPriv->radosFsPriv()->stat(mPriv->path, &mPriv->stat) == 0;

  if (!mPriv->exists)
    return;

  if (isLink())
  {
    const std::string &linkTarget = mPriv->stat.translatedPath;
    mPriv->target = linkTarget;
  }
}

int
RadosFsInfo::setXAttr(const std::string &attrName,
                      const std::string &value)
{
  // We don't call the similar methods from RadosFs for avoiding extra stat calls

  RadosFsPool *pool = mPriv->stat.pool.get();

  if (!pool)
    return -ENOENT;

  std::string &path = mPriv->stat.translatedPath;

  if (path == "")
    path = mPriv->stat.path;

  return setXAttrFromPath(pool->ioctx, mPriv->stat.statBuff,
                          mPriv->radosFs->uid(), mPriv->radosFs->gid(),
                          path, attrName, value);
}

int
RadosFsInfo::getXAttr(const std::string &attrName,
                      std::string &value,
                      size_t length)
{
  // We don't call the similar methods from RadosFs for avoiding extra stat calls

  RadosFsPool *pool = mPriv->stat.pool.get();

  if (!pool)
    return -ENOENT;

  std::string &path = mPriv->stat.translatedPath;

  if (path == "")
    path = mPriv->stat.path;

  return getXAttrFromPath(pool->ioctx, mPriv->stat.statBuff,
                          mPriv->radosFs->uid(), mPriv->radosFs->gid(),
                          path, attrName, value, length);
}

int
RadosFsInfo::removeXAttr(const std::string &attrName)
{
  // We don't call the similar methods from RadosFs for avoiding extra stat calls

  RadosFsPool *pool = mPriv->stat.pool.get();

  if (!pool)
    return -ENOENT;

  std::string &path = mPriv->stat.translatedPath;

  if (path == "")
    path = mPriv->stat.path;

  return removeXAttrFromPath(pool->ioctx, mPriv->stat.statBuff,
                             mPriv->radosFs->uid(), mPriv->radosFs->gid(),
                             path, attrName);
}

int
RadosFsInfo::getXAttrsMap(std::map<std::string, std::string> &map)
{
  // We don't call the similar methods from RadosFs for avoiding extra stat calls

  RadosFsPool *pool = mPriv->stat.pool.get();

  if (!pool)
    return -ENOENT;

  std::string &path = mPriv->stat.translatedPath;

  if (path == "")
    path = mPriv->stat.path;

  return getMapOfXAttrFromPath(pool->ioctx, mPriv->stat.statBuff,
                               mPriv->radosFs->uid(), mPriv->radosFs->gid(),
                               path, map);
}

int
RadosFsInfo::createLink(const std::string &linkName)
{
  std::string absLinkName(linkName);

  if (!exists())
  {
    radosfs_debug("Cannot create a link to a file or directly "
                  "that doesn't exist");
    return -ENOENT;
  }

  if (linkName == "")
  {
    radosfs_debug("The link name cannot be empty");
    return -EINVAL;
  }

  if (isLink())
  {
    radosfs_debug("Cannot make a link to a link");
    return -EPERM;
  }

  if (linkName[0] != PATH_SEP)
    absLinkName = getParentDir(mPriv->path, 0) + linkName;

  return mPriv->makeLink(absLinkName);
}

bool
RadosFsInfo::isLink() const
{
  if (!exists())
    return false;

  return S_ISLNK(mPriv->stat.statBuff.st_mode);
}

std::string
RadosFsInfo::targetPath() const
{
  return mPriv->target;
}

void *
RadosFsInfo::fsStat(void)
{
  return &mPriv->stat;
}

void
RadosFsInfo::setFsStat(void *stat)
{
  mPriv->stat = *reinterpret_cast<RadosFsStat *>(stat);
}

void *
RadosFsInfo::parentFsStat()
{
  return &mPriv->parentDirStat;
}

int
RadosFsInfo::chmod(long int permissions)
{
  return -EOPNOTSUPP;
}

int
RadosFsInfo::rename(const std::string &newPath)
{
  return -EOPNOTSUPP;
}

RADOS_FS_END_NAMESPACE
