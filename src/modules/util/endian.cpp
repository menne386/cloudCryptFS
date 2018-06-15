// Copyright 2018 Menne Kamminga <kamminga DOT m AT gmail DOT com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
#include "modules/util/endian.h"
#include "main.h"

void util::testEndianFunction(void) {

	/*Test wether swap_bytes really swaps the bytes*/
	auto x = util::swap_bytes(0xAABBCCDD);
	_ASSERT(x==0xDDCCBBAA);
	
	
	
}
