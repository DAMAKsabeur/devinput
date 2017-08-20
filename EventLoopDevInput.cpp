/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#define GIBBON_KEY(x) GIBBON_##x

#include <Screen.h>
#include <GibbonEvent.h>
#include <GibbonEventLoop.h>
#include <GibbonApplication.h>

#include <nrdbase/ScopedMutex.h>
#include <nrdbase/Time.h>
#include <nrdbase/Log.h>

#include <sys/select.h>
#include <linux/input.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
/*SAB*/ /*13 jun 2017*/
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#define DEV_INPUT_NAME "event0"
#define DEV_INPUT_SIZE (sizeof(DEV_INPUT_NAME)/sizeof(char))
/* SAB*/
using namespace netflix::gibbon;
using namespace netflix;

static inline KeyEvent::Key keyCodeToKey(uint16_t c)
{
  KeyEvent::Key code = KeyEvent::GIBBON_KEY_UNKNOWN;
  switch (c) {
	case KEY_SELECT: code = KeyEvent::GIBBON_KEY_RETURN; break;
    case KEY_ENTER: code = KeyEvent::GIBBON_KEY_RETURN; break;
    case KEY_OK: code = KeyEvent::GIBBON_KEY_RETURN; break;
    case KEY_INFO: code = KeyEvent::GIBBON_KEY_F5; break;
    case KEY_BACK: code = KeyEvent::GIBBON_KEY_ESCAPE; break;
    case KEY_END: code = KeyEvent::GIBBON_KEY_END; break;
    case KEY_HOME: code = KeyEvent::GIBBON_KEY_HOME; break;
    case KEY_F1: code = KeyEvent::GIBBON_KEY_F1; break;
    case KEY_F2: code = KeyEvent::GIBBON_KEY_F2; break;
    case KEY_F3: code = KeyEvent::GIBBON_KEY_F3; break;
    case KEY_F4: code = KeyEvent::GIBBON_KEY_F4; break;
    case KEY_F5: code = KeyEvent::GIBBON_KEY_F5; break;
    case KEY_F6: code = KeyEvent::GIBBON_KEY_F6; break;
    case KEY_F7: code = KeyEvent::GIBBON_KEY_F7; break;
    case KEY_F8: code = KeyEvent::GIBBON_KEY_F8; break;
    case KEY_F9: code = KeyEvent::GIBBON_KEY_F9; break;
    case KEY_F10: code = KeyEvent::GIBBON_KEY_F10; break;
    case KEY_F11: code = KeyEvent::GIBBON_KEY_F11; break;
    case KEY_F12: code = KeyEvent::GIBBON_KEY_F12; break;
    case KEY_F13: code = KeyEvent::GIBBON_KEY_F13; break;
    case KEY_F14: code = KeyEvent::GIBBON_KEY_F14; break;
    case KEY_F15: code = KeyEvent::GIBBON_KEY_F15; break;
    case KEY_F16: code = KeyEvent::GIBBON_KEY_F16; break;
    case KEY_F17: code = KeyEvent::GIBBON_KEY_F17; break;
    case KEY_F18: code = KeyEvent::GIBBON_KEY_F18; break;
    case KEY_F19: code = KeyEvent::GIBBON_KEY_F19; break;
    case KEY_F20: code = KeyEvent::GIBBON_KEY_F20; break;
    case KEY_F21: code = KeyEvent::GIBBON_KEY_F21; break;
    case KEY_F22: code = KeyEvent::GIBBON_KEY_F22; break;
    case KEY_F23: code = KeyEvent::GIBBON_KEY_F23; break;
    case KEY_F24: code = KeyEvent::GIBBON_KEY_F24; break;
    case KEY_LEFT: code = KeyEvent::GIBBON_KEY_LEFT; break;
    case KEY_UP: code = KeyEvent::GIBBON_KEY_UP; break;
    case KEY_RIGHT: code = KeyEvent::GIBBON_KEY_RIGHT; break;
    case KEY_DOWN: code = KeyEvent::GIBBON_KEY_DOWN; break;
    case KEY_BACKSPACE: code = KeyEvent::GIBBON_KEY_BACKSPACE; break;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT: code = KeyEvent::GIBBON_KEY_SHIFT; break;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL: code = KeyEvent::GIBBON_KEY_CONTROL; break;
    case KEY_LEFTALT:
    case KEY_RIGHTALT: code = KeyEvent::GIBBON_KEY_ALT; break;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA: code = KeyEvent::GIBBON_KEY_META; break;
    case KEY_CAPSLOCK: code = KeyEvent::GIBBON_KEY_CAPSLOCK; break;
    case KEY_NUMLOCK: code = KeyEvent::GIBBON_KEY_NUMLOCK; break;
    case KEY_SCROLLLOCK: code = KeyEvent::GIBBON_KEY_SCROLLLOCK; break;
    case KEY_ESC: code = KeyEvent::GIBBON_KEY_ESCAPE; break;
    case KEY_TAB: code = KeyEvent::GIBBON_KEY_TAB; break;
    case KEY_INSERT: code = KeyEvent::GIBBON_KEY_INSERT; break;
    case KEY_DELETE: code = KeyEvent::GIBBON_KEY_DELETE; break;
    case KEY_PRINT: code = KeyEvent::GIBBON_KEY_PRINT; break;
    case KEY_PAUSE: code = KeyEvent::GIBBON_KEY_PAUSE; break;

    case KEY_PLAY: code = KeyEvent::GIBBON_KEY_PLAY; break;
    case KEY_PLAYPAUSE: code = KeyEvent::GIBBON_KEY_PLAY; break;
    case KEY_FASTFORWARD: code = KeyEvent::GIBBON_KEY_MEDIA_FAST_FORWARD; break;
    case KEY_REWIND: code = KeyEvent::GIBBON_KEY_MEDIA_REWIND; break;
    case KEY_STOP: code = KeyEvent::GIBBON_KEY_MEDIA_STOP; break;
    case KEY_FIND: code = KeyEvent::GIBBON_KEY_BROWSER_SEARCH; break;
    case KEY_SEARCH: code = KeyEvent::GIBBON_KEY_BROWSER_SEARCH; break;
    case KEY_CLEAR: code = KeyEvent::GIBBON_KEY_BACKSPACE; break;
    default:
                    printf("EventLoopDevInput.cpp: Unknown key %d\n", c);
                    break;
  }
  return code;
}

// let's hope the keys in input.h don't change
static const char* ltxt = "\t\t1234567890-=\t\tqwertyuiop[]\t\tasdfghjkl;'`\t\\zxcvbnm,./\t\t\t ";
static const char* utxt = "\t\t!@#$%^&*()_+\t\tQWERTYUIOP{}\t\tASDFGHJKL:\"~\t|ZXCVBNM<>?\t\t\t ";

static inline std::string keyCodeToString(uint16_t code, bool shift)
{
  assert(KEY_1 == 2 && KEY_SPACE == 57);
  if (code >= KEY_1 && code <= KEY_SPACE && *(utxt + code) != '\t')
    return std::string(shift ? (utxt + code) : (ltxt + code), 1);
  return std::string();
}

class GibbonEventLoopDevInputPrivate
{
public:
  GibbonEventLoopDevInputPrivate();
  ~GibbonEventLoopDevInputPrivate();

  void rescan();
  virtual void wait(llong mseconds);
  virtual void wakeup();
  int mPipe[2];
  std::vector<int> mDevices;
  bool mShift;
  bool handleInput(int fd);
};

GibbonEventLoopDevInputPrivate::GibbonEventLoopDevInputPrivate()
{
  mShift = false;

  const int ret = ::pipe(mPipe);
  (void)ret;
  assert(!ret);

  rescan();
}

GibbonEventLoopDevInputPrivate::~GibbonEventLoopDevInputPrivate()
{
  close(mPipe[0]);
  close(mPipe[1]);
  for (std::vector<int>::const_iterator it = mDevices.begin(), end = mDevices.end();
       it != end; ++it) {
    close(*it);
  }
  mDevices.clear();
}

void GibbonEventLoopDevInputPrivate::rescan()
{
  for (std::vector<int>::const_iterator it = mDevices.begin(), end = mDevices.end();
       it != end; ++it) {
    printf("closing %d\n", *it);
    close(*it);
  }
  mDevices.clear();

  // find devices in /dev/input/
  int fd = -1 ;
  int rc = -1 ;
  DIR* dir = opendir("/dev/input");
  if (!dir) {
    Log::error(TRACE_EVENTLOOP, "No /dev/input directory");
    return;
  }
/* SAB */
  const long int namemax = fpathconf(dirfd(dir), _PC_NAME_MAX);
  if (namemax == -1) {
    closedir(dir);
    Log::error(TRACE_EVENTLOOP, "Unable to get max size of dir");
    return;
  }
  const size_t bufsize = std::max<long int>(namemax + offsetof(struct dirent, d_name),
                                            sizeof(struct dirent));
  dirent* buf = static_cast<dirent*>(malloc(bufsize));
  dirent* ent;

  while (readdir_r(dir, buf, &ent) == 0 && ent) {
    if (ent->d_type == DT_CHR) {
      const std::string name(ent->d_name);
      if (name.substr(0, DEV_INPUT_SIZE) == DEV_INPUT_NAME) {
		  
		  
        // open the device
        /*reset the device before attributing it to netflix*/
        fd = open(("/dev/input/" + name).c_str(), O_RDONLY);
        if (fd == -1) {
          Log::error(TRACE_EVENTLOOP, "Unable to open device %s", name.c_str());
          continue;
        }
        rc = ioctl(fd, USBDEVFS_RESET, 0);
        if (rc < 0) {
            perror("Error in ioctl");
            Log::error(TRACE_EVENTLOOP,"Reset fail\n");
        }
        Log::error(TRACE_EVENTLOOP,"Reset successful\n");
        close(fd);
        // open the device
         fd = open(("/dev/input/" + name).c_str(), O_RDONLY);
        if (fd == -1) {
          Log::error(TRACE_EVENTLOOP, "Unable to open device %s", name.c_str());
          continue;
        }
        mDevices.push_back(fd);
      }
    }
  }
  free(buf);
  closedir(dir);
}

bool GibbonEventLoopDevInputPrivate::handleInput(int fd)
{
	enum { NumEvents = 16 };

	input_event ev[NumEvents];
	int rd = ::read(fd, ev, sizeof(input_event) * NumEvents);
	if (rd <= 0)
	return false;
	unsigned int cur = 0;
	const int inputSize = sizeof(input_event);

	while (rd >= inputSize)
	{
		switch (ev[cur].type)
		{
			case EV_KEY:
			{
				const uint16_t code = ev[cur].code;
				const KeyEvent::Key key = keyCodeToKey(code);
				const InputEvent::Type type = !ev[cur].value ? InputEvent::Type_KeyRelease : InputEvent::Type_KeyPress;
				const bool repeat = ev[cur].value == 2;
				if (key == KeyEvent::GIBBON_KEY_SHIFT)
				{
					if (repeat)
						break;
					
					mShift = (type == InputEvent::Type_KeyPress);
				}

				printf("code %u %s\n", code, keyCodeToString(code, mShift).c_str());;
				GibbonApplication::instance()->sendEvent(new KeyEvent(type, key, keyCodeToString(code, mShift), 0, repeat));
				break;
			}
		}

		++cur;
		rd -= inputSize;
	}
 
	return (true);
}

void GibbonEventLoopDevInputPrivate::wait(llong mseconds)
{
  int waitUntil = mseconds == -1 ? 0 : mseconds;

  struct timeval tv;

  tv.tv_sec = waitUntil / 1000;
  tv.tv_usec = (waitUntil % 1000) * MicroSecondsPerMillisecond;
  fd_set readset;
  FD_ZERO(&readset);
  FD_SET(mPipe[0], &readset);

  int m = mPipe[0];

  // set up all the input devices
  for (std::vector<int>::const_iterator it = mDevices.begin(), end = mDevices.end();
       it != end; ++it) {
    FD_SET(*it, &readset);
    m = std::max(m, *it);
  }

  const int ret = select(m + 1, &readset, 0, 0, &tv);
  if (ret > 0) {
    if (FD_ISSET(mPipe[0], &readset)) {
      char buff;
      (void)read(mPipe[0], &buff, 1);
      return;
    }
    // find the devices to read from
    std::vector<int>::iterator it = mDevices.begin();
    while (it != mDevices.end()) {
      if (FD_ISSET(*it, &readset)) {
        if (!handleInput(*it)) {
          // fd closed?
          close(*it);
          it = mDevices.erase(it);
          continue;
        }
      }
      ++it;
    }
  } else if (ret == -1) {
    Log::warn(TRACE_EVENTLOOP, "EventLoopDevInput, select failure (%d), rescanning", errno);
    rescan();
  }

}

void GibbonEventLoopDevInputPrivate::wakeup()
{
  write(mPipe[1], " ", 1);
}

void GibbonEventLoop::moveMouse(const Point &)
{
}

bool GibbonEventLoop::hasEvents() const
{
  return false;
}

void GibbonEventLoop::wakeup()
{
    std::shared_ptr<GibbonEventLoopDevInputPrivate> priv;
    {
        ScopedMutex _lock(mMutex);
        priv = mPriv;
    }
    if(priv)
        priv->wakeup();
    EventLoop::wakeup();
}

void GibbonEventLoop::rescan()
{
  std::shared_ptr<GibbonEventLoopDevInputPrivate> priv;
  {
    ScopedMutex _lock(mMutex);
    priv = mPriv;
  }
  if(priv) {
    priv->rescan();
  }
}

void GibbonEventLoop::init()
{
}

void GibbonEventLoop::cleanup()
{
}

void GibbonEventLoop::startInput_sys()
{
    ScopedMutex _lock(mMutex);
    if(!mPriv)
        mPriv.reset(new GibbonEventLoopDevInputPrivate);
}


class ReleaseDevInputEventBufferEvent : public Application::Event
{
public:
    ReleaseDevInputEventBufferEvent(const std::shared_ptr<GibbonEventLoopDevInputPrivate> &priv) : mPriv(priv) { }

    virtual void eventCanceled() { eventFired(); }
    virtual void eventFired() { mPriv.reset(); }
    virtual std::string describe() const { return "ReleaseDevInputEventBufferEvent"; }
private:
    std::shared_ptr<GibbonEventLoopDevInputPrivate> mPriv;
};


void GibbonEventLoop::stopInput_sys()
{
  std::shared_ptr<GibbonEventLoopDevInputPrivate> priv;
    {
        ScopedMutex _lock(mMutex);
        std::swap(mPriv, priv);
    }
    if(priv)
        Animation::sendEvent(new ReleaseDevInputEventBufferEvent(priv));
}

void GibbonEventLoop::wait(llong mseconds)
{
  std::shared_ptr<GibbonEventLoopDevInputPrivate> priv;
  {
    ScopedMutex _lock(mMutex);
    priv = mPriv;
  }
  if(!priv) {
    EventLoop::wait(mseconds);
    return;
  }
  priv->wait(mseconds);
}

