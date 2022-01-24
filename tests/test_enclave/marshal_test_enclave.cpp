// Copyright 2021 Secretarium Ltd <contact@secretarium.org>

#include "stdio.h"
#include <string>
#include <cstring>
#include <cstdio>

#include "sgx_trts.h"
#include "trusted/enclave_marshal_test_t.h"
extern "C"
{
int enclave_marshal_test_init(void** zone_config, void** zone)
{
	return 0;
}

void enclave_marshal_test_destroy(void* zone)
{
	malloc(10);
}

int test(void** data_in, void** data_out)
{
	return 0;
}
}