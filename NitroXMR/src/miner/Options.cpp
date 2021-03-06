/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2016-2017 XMRig       <support@xmrig.com>
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <jansson.h>
#include <string.h>
#include <uv.h>


#ifdef _MSC_VER
#   include "getopt/getopt.h"
#else
#   include <getopt.h>
#endif


#include "Cpu.h"
#include "donate.h"
#include "net/Url.h"
#include "Options.h"
#include "Platform.h"
#include "version.h"


#ifndef ARRAY_SIZE
#   define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif


Options *Options::m_self = nullptr;


static char const usage[] = "\
Usage: " APP_ID " [OPTIONS]\n\
Options:\n\
  -a, --algo=ALGO       cryptonight (default) or cryptonight-lite\n\
  -o, --url=URL         URL of mining server\n\
  -O, --userpass=U:P    username:password pair for mining server\n\
  -u, --user=USERNAME   username for mining server\n\
  -p, --pass=PASSWORD   password for mining server\n\
  -t, --threads=N       number of miner threads\n\
  -v, --av=N            algorithm variation, 0 auto select\n\
  -k, --keepalive       send keepalived for prevent timeout (need pool support)\n\
  -r, --retries=N       number of times to retry before switch to backup server (default: 5)\n\
  -R, --retry-pause=N   time to pause between retries (default: 5)\n\
      --cpu-affinity    set process affinity to CPU core(s), mask 0x3 for cores 0 and 1\n\
      --cpu-priority    set process priority (0 idle, 2 normal to 5 highest)\n\
      --no-huge-pages   disable huge pages support\n\
      --no-color        disable colored output\n\
      --donate-level=N  donate level, default 5%% (5 minutes in 100 minutes)\n\
      --user-agent      set custom user-agent string for pool\n\
  -B, --background      run the miner in the background\n\
  -c, --config=FILE     load a JSON-format configuration file\n\
  -l, --log-file=FILE   log all output to a file\n"
# ifdef HAVE_SYSLOG_H
"\
  -S, --syslog          use system log for output messages\n"
# endif
"\
      --max-cpu-usage=N maximum CPU usage for automatic threads mode (default 75)\n\
      --safe            safe adjust threads and av settings for current CPU\n\
      --nicehash        enable nicehash support\n\
      --print-time=N    print hashrate report every N seconds\n\
  -h, --help            display this help and exit\n\
  -V, --version         output version information and exit\n\
";


static char const short_options[] = "a:c:khBp:Px:r:R:s:t:T:o:u:O:v:Vl:S";


static struct option const options[] = {
    { "algo",          1, nullptr, 'a'  },
    { "av",            1, nullptr, 'v'  },
    { "background",    0, nullptr, 'B'  },
    { "config",        1, nullptr, 'c'  },
    { "cpu-affinity",  1, nullptr, 1020 },
    { "cpu-priority",  1, nullptr, 1021 },
    { "donate-level",  1, nullptr, 1003 },
    { "help",          0, nullptr, 'h'  },
    { "keepalive",     0, nullptr ,'k'  },
    { "log-file",      1, nullptr, 'l'  },
    { "max-cpu-usage", 1, nullptr, 1004 },
    { "nicehash",      0, nullptr, 1006 },
    { "no-color",      0, nullptr, 1002 },
    { "no-huge-pages", 0, nullptr, 1009 },
    { "pass",          1, nullptr, 'p'  },
    { "print-time",    1, nullptr, 1007 },
    { "retries",       1, nullptr, 'r'  },
    { "retry-pause",   1, nullptr, 'R'  },
    { "safe",          0, nullptr, 1005 },
    { "syslog",        0, nullptr, 'S'  },
    { "threads",       1, nullptr, 't'  },
    { "url",           1, nullptr, 'o'  },
    { "user",          1, nullptr, 'u'  },
    { "user-agent",    1, nullptr, 1008 },
    { "userpass",      1, nullptr, 'O'  },
    { "version",       0, nullptr, 'V'  },
    { 0, 0, 0, 0 }
};


static struct option const config_options[] = {
    { "algo",          1, nullptr, 'a'  },
    { "av",            1, nullptr, 'v'  },
    { "background",    0, nullptr, 'B'  },
    { "colors",        0, nullptr, 2000 },
    { "cpu-affinity",  1, nullptr, 1020 },
    { "cpu-priority",  1, nullptr, 1021 },
    { "donate-level",  1, nullptr, 1003 },
    { "huge-pages",    0, nullptr, 1009 },
    { "log-file",      1, nullptr, 'l'  },
    { "max-cpu-usage", 1, nullptr, 1004 },
    { "print-time",    1, nullptr, 1007 },
    { "retries",       1, nullptr, 'r'  },
    { "retry-pause",   1, nullptr, 'R'  },
    { "safe",          0, nullptr, 1005 },
    { "syslog",        0, nullptr, 'S'  },
    { "threads",       1, nullptr, 't'  },
    { "user-agent",    1, nullptr, 1008 },
    { 0, 0, 0, 0 }
};


static struct option const pool_options[] = {
    { "url",           1, nullptr, 'o'  },
    { "pass",          1, nullptr, 'p'  },
    { "user",          1, nullptr, 'u'  },
    { "userpass",      1, nullptr, 'O'  },
    { "keepalive",     0, nullptr ,'k'  },
    { "nicehash",      0, nullptr, 1006 },
    { 0, 0, 0, 0 }
};


static const char *algo_names[] = {
    "cryptonight",
#   ifndef XMRIG_NO_AEON
    "cryptonight-lite"
#   endif
};


Options *Options::parse(int argc, char **argv)
{

    Options *options = new Options(argc, argv);
   //参数表初始化

	if (options->isReady()) {
        m_self = options;
        return m_self;
    }
	//否则说明参数不对
    delete options;
    return nullptr;
}

Options *Options::parse()
{
	Options *options = new Options();
	//参数表初始化

	if (options->isReady()) {
		m_self = options;
		return m_self;
	}
	//否则说明参数不对
	delete options;
	return nullptr;
}

const char *Options::algoName() const
{
    return algo_names[m_algo];
}

Options::Options() : //一堆参数初始化表
	m_background(false),
	m_colors(true),
	m_doubleHash(false),
	m_hugePages(true),
	m_ready(false),
	m_safe(false),
	m_syslog(false),
	m_logFile(nullptr),
	m_userAgent(nullptr),
	m_algo(0),
	m_algoVariant(0),
	m_donateLevel(kDonateLevel),
	m_maxCpuUsage(75),
	m_printTime(60),
	m_priority(-1),
	m_retries(5),
	m_retryPause(5),
	m_threads(0),
	m_affinity(-1L)
{
	if (SetArgs())
	{

	}


	if (!m_pools[0]->isValid()) {					//如果在参数没有指定矿池的情况下
		parseConfig(Platform::defaultConfigName()); //如果参数不对的话就加载这默认配置文件 config.json
	}

	if (!m_pools[0]->isValid()) {					//初始化之后还是不对就直接退出
		fprintf(stderr, "No pool URL supplied. Exiting.\n");
		return;
	}

	m_algoVariant = getAlgoVariant();
	if (m_algoVariant == AV2_AESNI_DOUBLE || m_algoVariant == AV4_SOFT_AES_DOUBLE) {
		m_doubleHash = true;
	}

	if (!m_threads) {
		m_threads = Cpu::optimalThreadsCount(m_algo, m_doubleHash, m_maxCpuUsage);
		//这里创建一个线程
	}
	else if (m_safe) {
		const int count = Cpu::optimalThreadsCount(m_algo, m_doubleHash, m_maxCpuUsage);
		if (m_threads > count) {
			m_threads = count;
		}
	}

	m_ready = true;
	//这里信息配置完毕
}

Options::Options(int argc, char **argv) : //一堆参数初始化表
    m_background(false),
    m_colors(true),
    m_doubleHash(false),
    m_hugePages(true),
    m_ready(false),
    m_safe(false),
    m_syslog(false),
    m_logFile(nullptr),
    m_userAgent(nullptr),
    m_algo(0),
    m_algoVariant(0),
    m_donateLevel(kDonateLevel),
    m_maxCpuUsage(75),
    m_printTime(60),
    m_priority(-1),
    m_retries(5),
    m_retryPause(5),
    m_threads(0),
    m_affinity(-1L)
{
	//m_pools 是个 vector的容器 是URL为模板
	//push_back:在向量末尾处添加一个元素。

/*************************************************************************/
	/*这段用于读取命令行,我们这里不需要配置*/
#if 0
    m_pools.push_back(new Url()); //填充地址 pool0
	int key;

    while (1) {
		//这里是比较重要的部分,对命令传入的参数进行识别
		//shortOP 是所有可能参数选项 op是参数表
		//参数匹配看不懂......
		key = getopt_long(argc, argv, short_options, options, NULL);
        if (key < 0) {
            break;
        }
		//上面算是吧这个opt给分开

		//这里根据参数名arg,参数值key 进行设置
        if (!parseArg(key, optarg)) {
            return;
        }
    }
    
	if (optind < argc) {
        fprintf(stderr, "%s: unsupported non-option argument '%s'\n", argv[0], argv[optind]);
        return;
    }
#endif
/****************************************************************************/
	if (SetArgs())
	{
		
	}


    if (!m_pools[0]->isValid()) {					//如果在参数没有指定矿池的情况下
        parseConfig(Platform::defaultConfigName()); //如果参数不对的话就加载这默认配置文件 config.json
    }

    if (!m_pools[0]->isValid()) {					//初始化之后还是不对就直接退出
        fprintf(stderr, "No pool URL supplied. Exiting.\n");
        return;
    }

    m_algoVariant = getAlgoVariant();
    if (m_algoVariant == AV2_AESNI_DOUBLE || m_algoVariant == AV4_SOFT_AES_DOUBLE) {
        m_doubleHash = true;
    }

    if (!m_threads) {
        m_threads = Cpu::optimalThreadsCount(m_algo, m_doubleHash, m_maxCpuUsage);
		//这里创建一个线程
    }
    else if (m_safe) {
        const int count = Cpu::optimalThreadsCount(m_algo, m_doubleHash, m_maxCpuUsage);
        if (m_threads > count) {
            m_threads = count;
        }
    }

    m_ready = true;
	//这里信息配置完毕
}

bool Options::SetArgs()
{
	//o
	
//	if (m_pools.size() > 1 || m_pools[0]->isValid()) {
		
		Url *url = new Url("mine.moneropool.com:3333");
		
		if (url->isValid()) {
			m_pools.push_back(url);
		}
		else {
			delete url;
		}
//	}
	//u
	m_pools.back()->setUser("42FWmAzd2vjZCTrdgAmuvtEoDxQnT6R25Dx7wdUDB7EdVDN4bJ2k4wV2gA3cBwmZKr53eD4P4pJgA8fC7dhZ3ZQePzZ8SUq");
	//p
	m_pools.back()->setPassword("x");
	//k
	parseBoolean('k', true);
	
	return TRUE;
}

Options::~Options()
{
}

//这里设置自定义参数
bool Options::parseArg(int key, const char *arg)
{
    switch (key) {
    case 'a': /* --algo */
        if (!setAlgo(arg)) {
            return false;
        }
        break;

    case 'o': /* --url */
        if (m_pools.size() > 1 || m_pools[0]->isValid()) {
            Url *url = new Url(arg);
            if (url->isValid()) {
                m_pools.push_back(url);
            }
            else {
                delete url;
            }
        }
        else {
            m_pools[0]->parse(arg);
        }

        if (!m_pools.back()->isValid()) {
            return false;
        }
        break;

    case 'O': /* --userpass */
        if (!m_pools.back()->setUserpass(arg)) {
            return false;
        }
        break;

    case 'u': /* --user */
        m_pools.back()->setUser(arg);
        break;

    case 'p': /* --pass */
        m_pools.back()->setPassword(arg);
        break;

    case 'l': /* --log-file */
        free(m_logFile);
        m_logFile = strdup(arg);
        m_colors = false;
        break;

    case 'r':  /* --retries */
    case 'R':  /* --retry-pause */
    case 't':  /* --threads */
    case 'v':  /* --av */
    case 1003: /* --donate-level */
    case 1004: /* --max-cpu-usage */
    case 1007: /* --print-time */
    case 1021: /* --cpu-priority */
        return parseArg(key, strtol(arg, nullptr, 10));

    case 'B':  /* --background */
    case 'k':  /* --keepalive */
    case 'S':  /* --syslog */
    case 1005: /* --safe */
    case 1006: /* --nicehash */
        return parseBoolean(key, true);

    case 1002: /* --no-color */
    case 1009: /* --no-huge-pages */
        return parseBoolean(key, false);

    case 'V': /* --version */
        showVersion();
        return false;

    case 'h': /* --help */
        showUsage(0);
        return false;

    case 'c': /* --config */
        parseConfig(arg);
        break;

    case 1020: { /* --cpu-affinity */
            const char *p  = strstr(arg, "0x");
            return parseArg(key, p ? strtoull(p, nullptr, 16) : strtoull(arg, nullptr, 10));
        }

    case 1008: /* --user-agent */
        free(m_userAgent);
        m_userAgent = strdup(arg);
        break;

    default:
        showUsage(1);
        return false;
    }

    return true;
}


bool Options::parseArg(int key, uint64_t arg)
{
    switch (key) {
        case 'r': /* --retries */
        if (arg < 1 || arg > 1000) {
            showUsage(1);
            return false;
        }

        m_retries = (int) arg;
        break;

    case 'R': /* --retry-pause */
        if (arg < 1 || arg > 3600) {
            showUsage(1);
            return false;
        }

        m_retryPause = (int) arg;
        break;

    case 't': /* --threads */
        if (arg < 1 || arg > 1024) {
            showUsage(1);
            return false;
        }

        m_threads = (int) arg;
        break;

    case 'v': /* --av */
        if (arg > 1000) {
            showUsage(1);
            return false;
        }

        m_algoVariant = (int) arg;
        break;

    case 1003: /* --donate-level */
        if (arg < 1 || arg > 99) {
            showUsage(1);
            return false;
        }

        m_donateLevel = (int) arg;
        break;

    case 1004: /* --max-cpu-usage */
        if (arg < 1 || arg > 100) {
            showUsage(1);
            return false;
        }

        m_maxCpuUsage = (int) arg;
        break;

    case 1007: /* --print-time */
        if (arg > 1000) {
            showUsage(1);
            return false;
        }

        m_printTime = (int) arg;
        break;

    case 1020: /* --cpu-affinity */
        if (arg) {
            m_affinity = arg;
        }
        break;

    case 1021: /* --cpu-priority */
        if (arg <= 5) {
            m_priority = (int) arg;
        }
        break;

    default:
        break;
    }

    return true;
}


bool Options::parseBoolean(int key, bool enable)
{
    switch (key) {
    case 'k': /* --keepalive */
        m_pools.back()->setKeepAlive(enable);
        break;

    case 'B': /* --background */
        m_background = enable;
        m_colors = enable ? false : m_colors;
        break;

    case 'S': /* --syslog */
        m_syslog = enable;
        m_colors = enable ? false : m_colors;
        break;

    case 1002: /* --no-color */
        m_colors = enable;
        break;

    case 1005: /* --safe */
        m_safe = enable;
        break;

    case 1006: /* --nicehash */
        m_pools.back()->setNicehash(enable);
        break;

    case 1009: /* --no-huge-pages */
        m_hugePages = enable;
        break;

    case 2000: /* colors */
        m_colors = enable;
        break;

    default:
        break;
    }

    return true;
}


Url *Options::parseUrl(const char *arg) const
{
    auto url = new Url(arg);
    if (!url->isValid()) {
        delete url;
        return nullptr;
    }

    return url;
}


void Options::parseConfig(const char *fileName)
{
    uv_fs_t req;
    const int fd = uv_fs_open(uv_default_loop(), &req, fileName, O_RDONLY, 0644, nullptr);
    if (fd < 0) {
        fprintf(stderr, "unable to open! %s: %s\n", fileName, uv_strerror(fd));
        return;
    }

    uv_fs_req_cleanup(&req);

    json_error_t err;
    json_t *config = json_loadfd(fd, 0, &err);

    uv_fs_close(uv_default_loop(), &req, fd, nullptr);
    uv_fs_req_cleanup(&req);

    if (!json_is_object(config)) {
        if (config) {
            json_decref(config);
            return;
        }

        if (err.line < 0) {
            fprintf(stderr, "%s\n", err.text);
        }
        else {
            fprintf(stderr, "%s:%d: %s\n", fileName, err.line, err.text);
        }

        return;
    }

    for (size_t i = 0; i < ARRAY_SIZE(config_options); i++) {
        parseJSON(&config_options[i], config);
		//根据json值配置参数
    }

    json_t *pools = json_object_get(config, "pools");
    if (json_is_array(pools)) {
        size_t index;
        json_t *value;

        json_array_foreach(pools, index, value) {
            if (json_is_object(value)) {
                for (size_t i = 0; i < ARRAY_SIZE(pool_options); i++) {
                    parseJSON(&pool_options[i], value);
					//配置矿池地址
                }
            }
        }
    }

    json_decref(config);
}


void Options::parseJSON(const struct option *option, json_t *object)
{
    if (!option->name) {
        return;
    }

    json_t *val = json_object_get(object, option->name);
    if (!val) {
        return;
    }

    if (option->has_arg && json_is_string(val)) {
        parseArg(option->val, json_string_value(val));
    }
    else if (option->has_arg && json_is_integer(val)) {
        parseArg(option->val, json_integer_value(val));
    }
    else if (!option->has_arg && json_is_boolean(val)) {
        parseBoolean(option->val, json_is_true(val));
    }
}


void Options::showUsage(int status) const
{
    if (status) {
        fprintf(stderr, "Try \"" APP_ID "\" --help' for more information.\n");
    }
    else {
        printf(usage);
    }
}


void Options::showVersion()
{
    printf(APP_NAME " " APP_VERSION "\n built on " __DATE__

#   if defined(__clang__)
    " with clang " __clang_version__);
#   elif defined(__GNUC__)
    " with GCC");
    printf(" %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#   elif defined(_MSC_VER)
    " with MSVC");
    printf(" %d", MSVC_VERSION);
#   else
    );
#   endif

    printf("\n features:"
#   if defined(__i386__) || defined(_M_IX86)
    " i386"
#   elif defined(__x86_64__) || defined(_M_AMD64)
    " x86_64"
#   endif

#   if defined(__AES__) || defined(_MSC_VER)
    " AES-NI"
#   endif
    "\n");

    printf("\nlibuv/%s\n", uv_version_string());
    printf("libjansson/%s\n", JANSSON_VERSION);
}


bool Options::setAlgo(const char *algo)
{
    for (size_t i = 0; i < ARRAY_SIZE(algo_names); i++) {
        if (algo_names[i] && !strcmp(algo, algo_names[i])) {
            m_algo = (int) i;
            break;
        }

#       ifndef XMRIG_NO_AEON
        if (i == ARRAY_SIZE(algo_names) - 1 && !strcmp(algo, "cryptonight-light")) {
            m_algo = ALGO_CRYPTONIGHT_LITE;
            break;
        }
#       endif

        if (i == ARRAY_SIZE(algo_names) - 1) {
            showUsage(1);
            return false;
        }
    }

    return true;
}


int Options::getAlgoVariant() const
{
#   ifndef XMRIG_NO_AEON
    if (m_algo == ALGO_CRYPTONIGHT_LITE) {
        return getAlgoVariantLite();
    }
#   endif

    if (m_algoVariant <= AV0_AUTO || m_algoVariant >= AV_MAX) {
        return Cpu::hasAES() ? AV1_AESNI : AV3_SOFT_AES;
    }

    if (m_safe && !Cpu::hasAES() && m_algoVariant <= AV2_AESNI_DOUBLE) {
        return m_algoVariant + 2;
    }

    return m_algoVariant;
}


#ifndef XMRIG_NO_AEON
int Options::getAlgoVariantLite() const
{
    if (m_algoVariant <= AV0_AUTO || m_algoVariant >= AV_MAX) {
        return Cpu::hasAES() ? AV2_AESNI_DOUBLE : AV4_SOFT_AES_DOUBLE;
    }

    if (m_safe && !Cpu::hasAES() && m_algoVariant <= AV2_AESNI_DOUBLE) {
        return m_algoVariant + 2;
    }

    return m_algoVariant;
}
#endif
