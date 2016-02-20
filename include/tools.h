/*
 * This file is part of evQueue
 * 
 * evQueue is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * evQueue is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with evQueue. If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Thibault Kummer <bob@coldsource.net>
 */

#ifndef _TOOLS_H_
#define _TOOLS_H_

int tools_queue_destroy();
int tools_queue_stats();
void tools_print_usage();
void tools_config_reload(void);
void tools_sync_tasks(void);
void tools_sync_notifications(void);
void tools_flush_retrier(void);
int tools_send_exit_msg(int type,int tid,char retcode);

#endif