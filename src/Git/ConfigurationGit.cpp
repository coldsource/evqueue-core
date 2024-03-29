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

#include <Git/ConfigurationGit.h>
#include <Exception/Exception.h>

#include <vector>

using namespace std;

namespace Git
{

static auto init = Configuration::GetInstance()->RegisterConfig(new ConfigurationGit());

ConfigurationGit::ConfigurationGit()
{
	entries["git.repository"] = "";
	entries["git.user"] = "";
	entries["git.password"] = "";
	entries["git.public_key"] = "";
	entries["git.private_key"] = "";
	entries["git.signature.name"] = "evQueue";
	entries["git.signature.email"] = "evqueue@local";
	entries["git.workflows.subdirectory"] = "workflows";
}

ConfigurationGit::~ConfigurationGit()
{
}

void ConfigurationGit::Check(void)
{
}

}
