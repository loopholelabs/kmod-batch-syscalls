/*
    Copyright (C) 2024 Loophole Labs

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef MEMORY_OVERLAY_LOG_H
#define MEMORY_OVERLAY_LOG_H

#define _log_prepend_crit "[memory_overlay  (CRIT)]:"
#define _log_prepend_error "[memory_overlay (ERROR)]:"
#define _log_prepend_warn "[memory_overlay  (WARN)]:"
#define _log_prepend_info "[memory_overlay  (INFO)]:"
#define _log_prepend_debug "[memory_overlay (DEBUG)]:"
#define _log_prepend_trace "[memory_overlay (TRACE)]:"
#define _log_prepend_benchmark "[memory_overlay (BENCH)]:"

#define log_crit(fmt, ...)                                             \
	printk(KERN_CRIT _log_prepend_crit " " fmt "\n" __VA_OPT__(, ) \
		       __VA_ARGS__)
#define log_error(fmt, ...)                                            \
	printk(KERN_ERR _log_prepend_error " " fmt "\n" __VA_OPT__(, ) \
		       __VA_ARGS__)
#define log_warn(fmt, ...)                                                \
	printk(KERN_WARNING _log_prepend_warn " " fmt "\n" __VA_OPT__(, ) \
		       __VA_ARGS__)
#define log_info(fmt, ...)                                             \
	printk(KERN_INFO _log_prepend_info " " fmt "\n" __VA_OPT__(, ) \
		       __VA_ARGS__)

#if LOG_LEVEL > 1
#define log_debug(fmt, ...)                                              \
	printk(KERN_DEBUG _log_prepend_debug " " fmt "\n" __VA_OPT__(, ) \
		       __VA_ARGS__)
#else
#define log_debug(fmt, ...)
#endif

#if LOG_LEVEL > 2
#define log_trace(fmt, ...)                                              \
	printk(KERN_DEBUG _log_prepend_trace " " fmt "\n" __VA_OPT__(, ) \
		       __VA_ARGS__)
#else
#define log_trace(fmt, ...)
#endif

#if LOG_LEVEL > 3
#define log_benchmark(fmt, ...)                                              \
	printk(KERN_DEBUG _log_prepend_benchmark " " fmt "\n" __VA_OPT__(, ) \
		       __VA_ARGS__)
#else
#define log_benchmark(fmt, ...)
#endif

#endif //MEMORY_OVERLAY_LOG_H
