#pragma once

// mini-scripting language implemented in qbe (mostly cvompatible with quake
// script engine
namespace cube {
namespace cmd {

// function signatures for script functions
enum {
  ARG_1INT, ARG_2INT, ARG_3INT, ARG_4INT,
  ARG_NONE,
  ARG_1STR, ARG_2STR, ARG_3STR, ARG_5STR,
  ARG_DOWN, ARG_DWN1,
  ARG_1EXP, ARG_2EXP,
  ARG_1EST, ARG_2EST,
  ARG_VARI
};
// register a console variable (done through globals)
int variable(const char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist);
// set the integer value for a variable
void setvar(const char *name, int i);
// set the value of a variable
int getvar(const char *name);
// says if the variable exists (i.e. is registered)
bool identexists(const char *name);
// register a new command
bool addcommand(const char *name, void (*fun)(), int narg);
// execute a given string
int execute(const char *p, bool down = true);
// execute a given file and print any error
void exec(const char *cfgfile);
// execute a file and says if this succeeded
bool execfile(const char *cfgfile);
// stop completion
void resetcomplete(void);
// complete the given string
void complete(char *s);
// set an alias with given action
void alias(const char *name, const char *action);
// get the action string for the given variable
char *getalias(const char *name);
// write all commands, variables and alias to config.cfg
void writecfg(void);
// free all resources needed by the command system
void clean(void);

} // namespace cmd
} // namespace cube

// register a command with a given name
#define COMMANDN(name, fun, nargs) \
  static bool __dummy_##fun = cube::cmd::addcommand(#name, (void (*)())fun, cmd::nargs)

// register a command with a name given by the function name
#define COMMAND(name, nargs) COMMANDN(name, name, nargs)

// a persistent variable
#define VARP(name, min, cur, max) \
  int name = cube::cmd::variable(#name, min, cur, max, &name, NULL, true)

// a non-persistent variable
#define VAR(name, min, cur, max) \
  int name = cube::cmd::variable(#name, min, cur, max, &name, NULL, false)

// a non-persistent variable with custom code to run when changed
#define VARF(name, min, cur, max, body) \
  void var_##name(); \
  static int name = cube::cmd::variable(#name, min, cur, max, &name, var_##name, false); \
  void var_##name() { body; }

// a persistent variable with custom code to run when changed
#define VARFP(name, min, cur, max, body) \
  void var_##name(); \
  static int name = cube::cmd::variable(#name, min, cur, max, &name, var_##name, true); \
  void var_##name() { body; }

