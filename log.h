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

#ifndef BATCH_SYSCALLS_LOG_H
#define BATCH_SYSCALLS_LOG_H

#define _log_prepend_info "[batch_mmap (INFO)]:"
#define _log_prepend_warn "[batch_mmap (WARN)]:"
#define _log_prepend_error "[batch_mmap (ERROR)]:"
#define _log_prepend_debug "[batch_mmap (DEBUG)]:"
#define _log_prepend_crit "[batch_mmap (CRIT)]:"

#define log_info(fmt, ...) printk(KERN_INFO _log_prepend_info " " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#define log_warn(fmt, ...) printk(KERN_WARNING _log_prepend_warn " " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#define log_error(fmt, ...) printk(KERN_ERR _log_prepend_error " " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#define log_crit(fmt, ...) printk(KERN_CRIT _log_prepend_crit " " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

#ifdef DEBUG
#define log_debug(fmt, ...) printk(KERN_DEBUG _log_prepend_debug " " fmt "\n" __VA_OPT__(,) __VA_ARGS__)
#else
#define log_debug(fmt, ...)
#endif

#endif //BATCH_SYSCALLS_LOG_H
