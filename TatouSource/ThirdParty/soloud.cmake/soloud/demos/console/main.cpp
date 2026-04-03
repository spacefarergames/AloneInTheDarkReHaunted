#include "soloud.h" // for SOLOUD_VERSION
#include "soloud_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstring>

#define TOSTR(x) #x
#define STRINGIFY(x) TOSTR(x)

class LCString
{
	std::string  original;
	std::string  data;

public:
	operator std::string& () { return data; }
	operator std::string const& () const { return data; }
	const char* c_str() const { return original.c_str(); }
	std::string& getOriginal() { return original; }
	bool operator==(const std::string& rhs) { return rhs == data; }
	bool operator==(const char * rhs) { return rhs == data; }

	void init(char const* s)
	{
		original = s;
		data = original;
		for (int i = 0; i < data.length(); i++)
			data[i] = tolower(data[i]);
	}

	LCString(char const* s)
	{
		init(s);
	}

	LCString(std::string s)
	{
		init(s.c_str());
	}
};

std::vector<LCString> split(const std::string& str, const std::string& delim)
{
	std::vector<LCString> tokens;
	size_t prev = 0, pos = 0;
	if (str[0] != ';')
	do
	{
		if (str[prev] == '"')
		{
			prev++;
			pos = str.find('"', prev);
			if (pos == std::string::npos) pos = str.length();
		}
		else
		{
			pos = str.find(delim, prev);
			if (pos == std::string::npos) pos = str.length();
		}
		std::string token = str.substr(prev, pos - prev);
		if (!token.empty()) tokens.push_back(token);
		prev = pos + delim.length();

	} while (pos < str.length() && prev < str.length());
	return tokens;
}

union rettype
{
	void* p;
	int i;
	double f;
};

std::unordered_map<LCString, rettype, std::hash<std::string>, std::equal_to<std::string> > gVar;

std::vector<LCString> gCmd;

typedef void (*funcptr)();

std::unordered_map<LCString, funcptr, std::hash<std::string>, std::equal_to<std::string> > gGlobalFuncs;
std::unordered_map<LCString, std::string, std::hash<std::string>, std::equal_to<std::string> > gMacros;

struct callStruct
{
	funcptr p;
	const char* info;
	int parcount;
	int varmask;
};

std::unordered_map<LCString, std::unordered_map<LCString, callStruct, std::hash<std::string>, std::equal_to<std::string> >, std::hash<std::string>, std::equal_to<std::string> > gSoloudCalls;

void prompt()
{
	printf("S> ");
}

int getVarOrInt(LCString& s)
{
	if (gVar.count(s) > 0)
	{
		return gVar[s].i;
	}
	return strtol(s.c_str(),0,10);
}

double getVarOrFloat(LCString& s)
{
	if (gVar.count(s) > 0)
	{
		return gVar[s].f;
	}
	return strtod(s.c_str(),0);
}

#include "generated.inc"

void cmd_quit()
{
	printf("Quiting..\n");
	exit(0);
}

void cmd_list()
{
	printf("Current set of variables:\n");
	int count = 0;
	for (auto& kv : gVar)
	{
		printf(" \"%s\" - 0x%08x\n", kv.first.c_str(), kv.second.i);
		count++;
	}
	printf("%d variables found\n", count);
	printf("Current set of macros:\n");
	count = 0;
	for (auto& kv : gMacros)
	{
		printf(" \"%s\" - \"%s\"\n", kv.first.c_str(), kv.second.c_str());
		count++;
	}
	printf("%d macros found\n", count);

}

void cmd_set()
{
	if (gCmd.size() != 3)
	{
		printf("set: 2 parameters expected\n");
		return;
	}
	gVar[gCmd[1]].i = getVarOrInt(gCmd[2]);
}

void cmd_unset()
{
	if (gCmd.size() != 2)
	{
		printf("unset: 1 parameter expected\n");
		return;
	}
	gVar.erase(gCmd[1]);
}

void run_cmd(char *cmdbuf);

void cmd_run()
{
	if (gCmd.size() != 2)
	{
		printf("run: 1 parameter expected\n");
		return;
	}
	FILE* f = fopen(gCmd[1].c_str(), "r");
	if (!f)
	{
		printf("run: \"%s\" not found\n", gCmd[1].c_str());
		return;
	}
	char fn[256];
	strcpy(fn, gCmd[1].c_str());
	char cmdbuf[256];
	int cp = 0;
	while (!feof(f))
	{
		int v = fgetc(f);
		if (v == '\n' || v == '\r' || v == EOF)
		{
			if (cp > 0)
			{
				cmdbuf[cp] = 0;
				cp = 0;				
				printf("%s> %s\n", fn, cmdbuf);
				gCmd = split(cmdbuf, " ");
				run_cmd(cmdbuf);
			}
		}
		else
		{
			cmdbuf[cp] = v;
			cp++;
		}
	}
	fclose(f);	
}

void cmd_define()
{
	if (gCmd.size() != 3)
	{
		printf("define: 2 parameters expected\n");
		return;
	}
	gMacros[gCmd[1]] = gCmd[2];
}


void cmd_help()
{
	if (gCmd.size() == 1)
	{
		printf("Global commands: help, quit, list, set, unset, run, define (use \"help cmd\" for more info)\n");
		printf("General command syntax:\n DEST OBJ FUNC PARAM PARAM PARAM..\nwhere:\n DEST is destination variable name\n OBJ is a soloud object type\n FUNC is function in the object\n PARAM is a parameter to the function\n");
		printf("Objects (do \"help obj\" for more info on the object):\n ");
		for (auto& kv : gSoloudCalls)
		{
			printf("%s ", kv.first.c_str());
		}
		printf("\n");
		return;
	}

	if (gCmd.size() == 2)
	{
		if (gCmd[1] == "help")
		{
			printf("Just use help without parameters for help on help.\n");
			return;
		}
		if (gCmd[1] == "quit")
		{
			printf("Use quit to exit the SoLoud console.\n");
			return;
		}
		if (gCmd[1] == "list")
		{
			printf("List currently existing variables and macros.\n");
			return;
		}
		if (gCmd[1] == "set")
		{
			printf("Set a variable to an integer value. Example:\n set answer 42\n");
			return;
		}
		if (gCmd[1] == "unset")
		{
			printf("Remove a variable. Example:\n unset begone\n");
			return;
		}
		if (gCmd[1] == "run")
		{
			printf("Run commands from a text file. Example:\n run foo.txt\n");
			return;
		}
		if (gCmd[1] == "define")
		{
			printf("Define a macro. Example:\n define bang \"0 soloud play s gunshot\"\n");
			return;
		}
		if (gSoloudCalls.count(gCmd[1]) == 0)
		{
			printf("Unknown command or object \"%s\"\n", gCmd[1].c_str());
			return;
		}
		printf("Object \"%s\" has the following functions:\n", gCmd[1].c_str());
		for (auto& kv : gSoloudCalls[gCmd[1]])
		{
			printf("%s ", kv.first.c_str());
		}
		printf("\n");
		return;
	}

	if (gCmd.size() == 3)
	{
		if (gSoloudCalls.count(gCmd[1]) == 0)
		{
			printf("Unknown object \"%s\"\n", gCmd[1].c_str());
			return;
		}
		if (gSoloudCalls[gCmd[1]].count(gCmd[2]) == 0)
		{
			printf("Object \"%s\" doesn't have function \"%s\"\n", gCmd[1].c_str(), gCmd[2].c_str());
			return;
		}
		printf("Object \"%s\" function \"%s\" info:\n %s\n(See SoLoud documentation for details)\n", gCmd[1].c_str(), gCmd[2].c_str(), gSoloudCalls[gCmd[1]][gCmd[2]].info);
		return;
	}
	printf("Unexpected number of parameters\n");
}


void setup()
{
	gGlobalFuncs["quit"] = cmd_quit;
	gGlobalFuncs["help"] = cmd_help;
	gGlobalFuncs["list"] = cmd_list;
	gGlobalFuncs["set"] = cmd_set;
	gGlobalFuncs["unset"] = cmd_unset;
	gGlobalFuncs["run"] = cmd_run;
	gGlobalFuncs["define"] = cmd_define;
	setup_soloud_calls();
}

void run_cmd(char *cmdbuf)
{
	if (gCmd.size() > 0)
	{
		if (gGlobalFuncs.count(gCmd[0]) != 0)
		{
			gGlobalFuncs[gCmd[0]]();
		}
		else
		{
			if (gCmd.size() == 1 && gMacros.count(gCmd[0]) != 0)
			{
				strcpy(cmdbuf, gMacros[gCmd[0]].c_str());
				gCmd = split(cmdbuf, " ");
				run_cmd(cmdbuf);
				return;
			}

			if (gCmd.size() < 3)
			{
				printf("Unknown command \"%s\" (try help)\n", cmdbuf);
			}
			else
			{
				if (gSoloudCalls.count(gCmd[1]) == 0)
				{
					printf("Unknown object \"%s\"\n", gCmd[1].c_str());
				}
				else
					if (gSoloudCalls[gCmd[1]].count(gCmd[2]) == 0)
					{
						printf("Object \"%s\" doesn't have function \"%s\"\n", gCmd[1].c_str(), gCmd[2].c_str());
					}
					else
					{
						int ok = 1;
						if (gCmd.size() != 3 + gSoloudCalls[gCmd[1]][gCmd[2]].parcount) {
							printf("Invalid number of parameters (%d expected)\n", gSoloudCalls[gCmd[1]][gCmd[2]].parcount);
							ok = 0;
						}
						else
						{
							for (int i = 0; i < gSoloudCalls[gCmd[1]][gCmd[2]].parcount; i++)
							{
								if ((gSoloudCalls[gCmd[1]][gCmd[2]].varmask & (1 << i)) != 0 && gVar.count(gCmd[3 + i]) == 0)
								{
									printf("Variable \"%s\" not found\n", gCmd[3].c_str());
									ok = 0;
								}
							}
						}

						if (ok)
						{
							gSoloudCalls[gCmd[1]][gCmd[2]].p();
						}
					}
			}
		}
	}
}

int main(int parc, char** pars)
{
	setup();
	char cmdbuf[256];
	printf("SoLoud console " STRINGIFY(SOLOUD_VERSION) "\nType \"help\" for help, \"quit\" to quit\n");
	prompt();
	while (1)
	{
		int p = 0;
		int c = 0;
		while (c != '\n' && p < 255)
		{
			c = getchar();
			if (c != '\n')
			{
				cmdbuf[p] = c;
				p++;
				cmdbuf[p] = 0;
			}
		}
		cmdbuf[p] = 0;
		gCmd = split(cmdbuf, " ");
		run_cmd(cmdbuf);
/*
		for (auto x : gCmd)
		{
			printf("\"%s\"\n", x.c_str());
		}
		*/
		prompt();
	}
	return 0;
}