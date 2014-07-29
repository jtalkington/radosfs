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

#ifndef RADOS_FS_OP_HH
#define RADOS_FS_OP_HH

#include <cstdlib>
#include <rados/librados.h>
#include <string>
#include <vector>
#include <tr1/memory>

#include "RadosFs.hh"
#include "radosfscommon.h"

#define FILE_STRIPE_LOCKER "file-stripe-locker"
#define FILE_STRIPE_LOCKER_COOKIE_WRITE "file-stripe-locker-cookie-write"
#define FILE_STRIPE_LOCKER_COOKIE_OTHER "file-stripe-locker-cookie-other"
#define FILE_STRIPE_LOCKER_TAG "file-stripe-locker-tag"

RADOS_FS_BEGIN_NAMESPACE

class RadosFsIO
{
public:
  RadosFsIO(RadosFs *radosFs,
            const RadosFsPoolSP pool,
            const std::string &iNode,
            size_t stripeSize,
            bool hasAligment);
  ~RadosFsIO();

  ssize_t read(char *buff, off_t offset, size_t blen);
  int write(const char *buff, off_t offset, size_t blen);
  int writeSync(const char *buff, off_t offset, size_t blen);

  std::string inode(void) const { return mInode; }

  void setLazyRemoval(bool remove) { mLazyRemoval = remove; }
  bool lazyRemoval(void) const { return mLazyRemoval; }

  std::string getStripePath(off_t offset) const;

  size_t stripeSize(void) const { return mStripeSize; }

  size_t getLastStripeIndex(void) const;

  int remove(void);

private:
  RadosFs *mRadosFs;
  const RadosFsPoolSP mPool;
  const std::string mInode;
  size_t mStripeSize;
  bool mLazyRemoval;
  bool mHasAlignment;
  std::vector<rados_completion_t> mCompletionList;

  void sync(void);
  void cleanCompletion(bool sync = false);
};

RADOS_FS_END_NAMESPACE

#endif /* RADOS_FS_OP_HH */
