#include <video.hpp>
#include <fstream>
namespace NextVideo {

#ifdef __EMSCRIPTEN__

const char* readFile(const char* path) {

  std::ifstream file(path);
  std::string   content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  char* data = new char[content.size() + 1];
  for (int i = 0; i < content.size(); i++) data[i] = content[i];
  return data;
}
#else
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
const char* readFile(const char* path) {
  struct stat _stat;
  stat(path, &_stat);

  if (_stat.st_size <= 0)
    return 0;

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return 0;
  }

  char* buffer          = (char*)malloc(_stat.st_size + 1);
  buffer[_stat.st_size] = 0;

  int current = 0;
  int size    = _stat.st_size;
  int step    = 0;

  while ((step = read(fd, &buffer[current], size - current))) {
    current += step;
  }

  return buffer;
}
#endif
} // namespace NextVideo
