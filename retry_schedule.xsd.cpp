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

#include <string>

std::string retry_schedule_xsd_str = "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \
 \
<xs:schema version=\"1.0\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" elementFormDefault=\"qualified\"> \
	 \
	<xs:element name=\"schedule\" type=\"scheduleType\" /> \
	 \
	<xs:complexType name=\"scheduleType\"> \
		<xs:sequence> \
			<xs:element name=\"level\" type=\"levelType\" maxOccurs=\"unbounded\" /> \
		</xs:sequence> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"levelType\"> \
		<xs:attribute name=\"retry_delay\" type=\"xs:positiveInteger\" use=\"required\" /> \
		<xs:attribute name=\"retry_times\" type=\"xs:positiveInteger\" use=\"required\" /> \
	</xs:complexType> \
	 \
	 \
	<xs:simpleType name=\"StrNonEmpty\"> \
		<xs:restriction base=\"xs:string\"> \
			<xs:minLength value=\"1\"/> \
		</xs:restriction> \
	</xs:simpleType>	 \
	 \
</xs:schema> \
";
