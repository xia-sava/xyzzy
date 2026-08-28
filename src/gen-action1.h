void dpp (int argc, char **argv);
void gen_ctab (int argc, char **argv);
void gen_fontrange (int argc, char **argv);
void gen_ibmext (int argc, char **argv);
void gen_iso2022state (int argc, char **argv);
void gen_ktab (int argc, char **argv);
void gen_lucida_width (int argc, char **argv);
void gen_syms (int argc, char **argv);
void gen_ucs2tab (int argc, char **argv);
void gen_utf2sjis (int argc, char **argv);

const gensrc_action actions[] =
{
  {"dpp", dpp},
  {"gen-ctab", gen_ctab},
  {"gen-fontrange", gen_fontrange},
  {"gen-ibmext", gen_ibmext},
  {"gen-iso2022state", gen_iso2022state},
  {"gen-ktab", gen_ktab},
  {"gen-lucida-width", gen_lucida_width},
  {"gen-syms", gen_syms},
  {"gen-ucs2tab", gen_ucs2tab},
  {"gen-utf2sjis", gen_utf2sjis},
};
