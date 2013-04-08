#pragma once
namespace cube {
namespace browser {

  void addserver(const char *servername);
  const char *getservername(int n);
  void refreshservers(void);
  void writeservercfg(void);

} // namespace browser
} // namespace cube

