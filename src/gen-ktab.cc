#include "gen-stdafx.h"

typedef unsigned short u_short;
typedef unsigned int u_int;

#if 0
static const WCHAR h_ascii[] = (L" !\"#$%&'()*+,-./"
                                L"0123456789:;<=>?"
                                L"@ABCDEFGHIJKLMNO"
                                L"PQRSTUVWXYZ[\\]^_"
                                L"`abcdefghijklmno"
                                L"pqrstuvwxyz{|}~");

//                              0123456789abcdef
static const WCHAR h_kana[] = ( L"｡｢｣､･ｦｧｨｩｪｫｬｭｮｯ"   // FF6
                               L"ｰｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿ"   // FF7
                               L"ﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏ"   // FF8
                               L"ﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝﾞﾟ"); // FF9
#endif

/* 半角の符号位置の順に、対応する全角の文字を並べる。空白は対応が無いことを表す */

static const WCHAR f_ascii[] = (L"　！”＃＄％＆’（）＊＋，－．／"
                                L"０１２３４５６７８９：；＜＝＞？"
                                L"＠ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯ"
                                L"ＰＱＲＳＴＵＶＷＸＹＺ［￥］＾＿"
                                L"‘ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏ"
                                L"ｐｑｒｓｔｕｖｗｘｙｚ｛｜｝￣");

static const WCHAR fk_kana[] = (L"。「」、・ヲァィゥェォャュョッ"
                                L"ーアイウエオカキクケコサシスセソ"
                                L"タチツテトナニヌネノハヒフヘホマ"
                                L"ミムメモヤユヨラリルレロワン゛゜");

static const WCHAR fh_kana[] = (L"。「」、・をぁぃぅぇぉゃゅょっ"
                                L"ーあいうえおかきくけこさしすせそ"
                                L"たちつてとなにぬねのはひふへほま"
                                L"みむめもやゆよらりるれろわん゛゜");

static const WCHAR fh_voiced[] = (L"がぎぐげござじずぜぞだぢづでど"
				  L"     ばびぶべぼ");

static const WCHAR fk_voiced[] = (L"ヴ  ガギグゲゴザジズゼゾ"
				  L"ダヂヅデド     バビブベボ");

static const WCHAR fh_semi_voiced[] = L"ぱぴぷぺぽ";

static const WCHAR fk_semi_voiced[] = L"パピプペポ";

/* 半角の側の符号位置 */
#define ASCII_MIN 0x0020
#define ASCII_MAX 0x007e
#define KANA_MIN 0xff61                 /* ｡ */
#define KANA_MAX 0xff9f                 /* ﾟ */
#define HIRA_VOICED_MIN 0xff76          /* ｶ */
#define KATA_VOICED_MIN 0xff73          /* ｳ */
#define VOICED_MAX 0xff8e               /* ﾎ */
#define SEMI_VOICED_MIN 0xff8a          /* ﾊ */
#define SEMI_VOICED_MAX 0xff8e          /* ﾎ */
#define VOICED_MARK 0xff9e              /* ﾞ */
#define SEMI_VOICED_MARK 0xff9f         /* ﾟ */

/* 全角の側は三つの面に散らばり、それぞれ 256 個に収まる */
static const u_short pages[] = {0x2000, 0x3000, 0xff00};

/* 濁点と半濁点の付いた仮名は仮名の面にしかない */
#define VOICED_PAGE 0x3000

static void
put_full (const char *name, const WCHAR *tab, int from, int to)
{
  printf ("static const Char %s[] = {\n", name);
  for (int i = from, j = 0; i <= to; i++, j++)
    {
      WCHAR c = tab[i - from];
      printf ("0x%04x,", c == ' ' ? 0 : c);
      if ((j & 7) == 7)
        printf ("\n");
    }
  printf ("};\n\n");
}

static void
fill_half (u_short *buf, u_short page)
{
  memset (buf, 0, sizeof *buf * 256);
  for (int i = ASCII_MIN; i <= ASCII_MAX; i++)
    if ((f_ascii[i - ASCII_MIN] & 0xff00) == page)
      buf[f_ascii[i - ASCII_MIN] & 0xff] = i;
  for (int i = KANA_MIN; i <= KANA_MAX; i++)
    {
      if ((fk_kana[i - KANA_MIN] & 0xff00) == page)
        buf[fk_kana[i - KANA_MIN] & 0xff] = i;
      if ((fh_kana[i - KANA_MIN] & 0xff00) == page)
        buf[fh_kana[i - KANA_MIN] & 0xff] = i;
    }
}

static void
put_half (const u_short *buf, u_short page)
{
  int min, max;

  for (min = 0; min < 256 && !buf[min]; min++)
    ;
  for (max = 255; max > min && !buf[max]; max--)
    ;
  if (min == max)
    return;
  printf ("#define TO_HALF_WIDTH%02x_MIN 0x%04x\n", page >> 8, page + min);
  printf ("#define TO_HALF_WIDTH%02x_MAX 0x%04x\n", page >> 8, page + max);
  printf ("static const Char to_half_width_%02x[] = {\n", page >> 8);
  for (int i = min, j = 0; i <= max; i++, j++)
    {
      printf ("0x%04x,", buf[i]);
      if ((j & 7) == 7)
        printf ("\n");
    }
  printf ("};\n\n");
}

static void
add_voiced (u_short *base, u_short *mark, const WCHAR *tab,
            int from, int to, u_short m)
{
  for (int i = from; i <= to; i++)
    {
      WCHAR c = tab[i - from];
      if (c != ' ' && (c & 0xff00) == VOICED_PAGE)
        {
          base[c & 0xff] = i;
          mark[c & 0xff] = m;
        }
    }
}

static void
put_voiced (void)
{
  u_short base[256], mark[256];
  int min, max;

  memset (base, 0, sizeof base);
  memset (mark, 0, sizeof mark);
  add_voiced (base, mark, fh_voiced, HIRA_VOICED_MIN, VOICED_MAX, VOICED_MARK);
  add_voiced (base, mark, fk_voiced, KATA_VOICED_MIN, VOICED_MAX, VOICED_MARK);
  add_voiced (base, mark, fh_semi_voiced, SEMI_VOICED_MIN, SEMI_VOICED_MAX,
              SEMI_VOICED_MARK);
  add_voiced (base, mark, fk_semi_voiced, SEMI_VOICED_MIN, SEMI_VOICED_MAX,
              SEMI_VOICED_MARK);

  for (min = 0; min < 256 && !base[min]; min++)
    ;
  for (max = 255; max > min && !base[max]; max--)
    ;
  printf ("struct voiced_sound {Char base, mark;};\n\n");
  printf ("#define VOICED_SOUND%02x_MIN 0x%04x\n",
          VOICED_PAGE >> 8, VOICED_PAGE + min);
  printf ("#define VOICED_SOUND%02x_MAX 0x%04x\n",
          VOICED_PAGE >> 8, VOICED_PAGE + max);
  printf ("static const voiced_sound voiced_sound_%02x[] = {\n",
          VOICED_PAGE >> 8);
  for (int i = min, j = 0; i <= max; i++, j++)
    {
      printf ("{0x%04x,0x%04x},", base[i], mark[i]);
      if ((j & 3) == 3)
        printf ("\n");
    }
  printf ("};\n\n");
}

void
gen_ktab (int argc, char **argv)
{
  u_short buf[256];

  put_full ("to_full_ascii", f_ascii, ASCII_MIN, ASCII_MAX);
  put_full ("to_fullkata", fk_kana, KANA_MIN, KANA_MAX);
  put_full ("to_fullhira", fh_kana, KANA_MIN, KANA_MAX);
  put_full ("to_fullhira_voiced", fh_voiced, HIRA_VOICED_MIN, VOICED_MAX);
  put_full ("to_fullkata_voiced", fk_voiced, KATA_VOICED_MIN, VOICED_MAX);
  put_full ("to_fullhira_semi_voiced", fh_semi_voiced,
            SEMI_VOICED_MIN, SEMI_VOICED_MAX);
  put_full ("to_fullkata_semi_voiced", fk_semi_voiced,
            SEMI_VOICED_MIN, SEMI_VOICED_MAX);

  printf ("#define TO_FULL_ASCII_MIN 0x%04x\n", ASCII_MIN);
  printf ("#define TO_FULL_ASCII_MAX 0x%04x\n", ASCII_MAX);
  printf ("#define TO_FULL_KANA_MIN 0x%04x\n", KANA_MIN);
  printf ("#define TO_FULL_KANA_MAX 0x%04x\n", KANA_MAX);
  printf ("#define TO_FULLHIRA_VOICED_MIN 0x%04x\n", HIRA_VOICED_MIN);
  printf ("#define TO_FULLKATA_VOICED_MIN 0x%04x\n", KATA_VOICED_MIN);
  printf ("#define TO_FULL_VOICED_MAX 0x%04x\n", VOICED_MAX);
  printf ("#define TO_FULL_SEMI_VOICED_MIN 0x%04x\n", SEMI_VOICED_MIN);
  printf ("#define TO_FULL_SEMI_VOICED_MAX 0x%04x\n", SEMI_VOICED_MAX);
  printf ("#define VOICED_SOUND_MARK 0x%04x\n", fk_kana[VOICED_MARK - KANA_MIN]);
  printf ("#define SEMI_VOICED_SOUND_MARK 0x%04x\n",
          fk_kana[SEMI_VOICED_MARK - KANA_MIN]);
  printf ("\n");

  for (int i = 0; i < sizeof pages / sizeof *pages; i++)
    {
      fill_half (buf, pages[i]);
      put_half (buf, pages[i]);
    }

  put_voiced ();

  printf ("#define FULL_WIDTH_HIRAGANA_MIN 0x3041\n");  /* ぁ */
  printf ("#define FULL_WIDTH_HIRAGANA_MAX 0x3093\n");  /* ん */
  printf ("#define FULL_WIDTH_KATAKANA_MIN 0x30a1\n");  /* ァ */
  printf ("#define FULL_WIDTH_KATAKANA_MAX 0x30f6\n");  /* ヶ */

  exit (0);
}
