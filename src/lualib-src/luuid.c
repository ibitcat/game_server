#include <uuid.h>
#include <stdio.h>

#include "luuid.h"
#include "lbase64.h"

static int luuid(lua_State *L){
	// uuid
	uuid_t uu;
	uuid_generate(uu);

	size_t outSize;
	unsigned char *uuidstr = base64_encode(uu, sizeof(uu), &outSize);
	uuidstr[22] = '\0';
	for (int i = 0; i < 22; ++i){
		if (*(uuidstr+i)=='/'){
			*(uuidstr+i) = '-'; // 替换"/"
		}
	}
	lua_pushlstring(L,uuidstr,22);
	return 1;
}

int luaopen_uuid(struct lua_State* L){
	luaL_checkversion(L);

	luaL_Reg l[] = {
		{ "uuid", luuid },
		{ NULL,  NULL }
	};
	//luaL_newlib(L,l);
	(luaL_checkversion(L), luaL_newlibtable(L,l), luaL_setfuncs(L,l,0));

	return 1;
}