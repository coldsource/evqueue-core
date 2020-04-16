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

std::string workflow_xsd_str = "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \
 \
<xs:schema version=\"1.0\" \
					 xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" \
					 elementFormDefault=\"qualified\"> \
	 \
	 \
	<xs:element name=\"workflow\" type=\"workflowType\" /> \
	 \
	 \
	<xs:complexType name=\"workflowType\"> \
		<xs:sequence> \
			<xs:element name=\"parameters\" type=\"parametersType\" minOccurs=\"0\" /> \
			<xs:element name=\"custom-attributes\" type=\"customattributesType\" minOccurs=\"0\" /> \
			<xs:element name=\"subjobs\" type=\"subjobsType\" /> \
		</xs:sequence> \
		<xs:attribute name=\"version\" type=\"xs:string\" use=\"optional\" /> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"parametersType\"> \
		<xs:sequence> \
			<xs:element name=\"parameter\" type=\"parameterType\" minOccurs=\"0\" maxOccurs=\"unbounded\" /> \
		</xs:sequence> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"parameterType\"> \
		<xs:attribute name=\"name\" type=\"xs:string\" use=\"required\" /> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"customattributesType\"> \
		<xs:sequence> \
			<xs:element name=\"custom-attribute\" type=\"customattributeType\" minOccurs=\"0\" maxOccurs=\"unbounded\" /> \
		</xs:sequence> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"customattributeType\"> \
		<xs:attribute name=\"name\" type=\"xs:string\" use=\"required\" /> \
		<xs:attribute name=\"select\" type=\"xs:string\" use=\"required\" /> \
		<xs:attribute name=\"description\" type=\"xs:string\" use=\"optional\" /> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"jobType\"> \
		<xs:sequence> \
			<xs:element name=\"tasks\" type=\"tasksType\" /> \
			<xs:element name=\"subjobs\" type=\"subjobsType\" minOccurs=\"0\"/> \
		</xs:sequence> \
		<xs:attribute name=\"name\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"loop\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"condition\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"iteration-condition\" type=\"xs:string\" use=\"optional\" /> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"tasksType\"> \
		<xs:sequence> \
			<xs:element name=\"task\" type=\"taskType\" maxOccurs=\"unbounded\" /> \
		</xs:sequence> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"taskType\"> \
		<xs:sequence> \
			<xs:element name=\"input\" type=\"inputType\" minOccurs=\"0\" maxOccurs=\"unbounded\" /> \
			<xs:element name=\"stdin\" type=\"stdinType\" minOccurs=\"0\" maxOccurs=\"1\" /> \
			<xs:element name=\"script\" type=\"scriptType\" minOccurs=\"0\" maxOccurs=\"1\" /> \
		</xs:sequence> \
		<xs:attribute name=\"type\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"name\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"path\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"wd\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"parameters-mode\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"output-method\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"merge-stderr\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"use-agent\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"queue\" type=\"xs:string\" use=\"required\" /> \
		<xs:attribute name=\"user\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"host\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"queue_host\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"retry_retval\" type=\"xs:integer\" use=\"optional\" /> \
		<xs:attribute name=\"retry_delay\" type=\"xs:positiveInteger\" use=\"optional\" /> \
		<xs:attribute name=\"retry_times\" type=\"xs:positiveInteger\" use=\"optional\" /> \
		<xs:attribute name=\"retry_schedule\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"loop\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"condition\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"iteration-condition\" type=\"xs:string\" use=\"optional\" /> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"inputType\" mixed=\"true\"> \
		<xs:choice minOccurs=\"0\" maxOccurs=\"unbounded\"> \
			<xs:element name=\"value\" type=\"valueType\" /> \
			<xs:element name=\"copy\" type=\"valueType\" /> \
		</xs:choice>       \
		<xs:attribute name=\"name\" type=\"StrNonEmpty\" use=\"optional\" /> \
		<xs:attribute name=\"loop\" type=\"xs:string\" use=\"optional\" /> \
		<xs:attribute name=\"condition\" type=\"xs:string\" use=\"optional\" /> \
	</xs:complexType> \
	 \
	<xs:complexType name=\"stdinType\" mixed=\"true\"> \
    <xs:choice minOccurs=\"0\" maxOccurs=\"unbounded\"> \
      <xs:element name=\"value\" type=\"valueType\" /> \
			<xs:element name=\"copy\" type=\"valueType\" /> \
    </xs:choice>       \
		<xs:attribute name=\"mode\" type=\"StrNonEmpty\" use=\"required\" /> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"valueType\"> \
		<xs:attribute name=\"select\" type=\"xs:string\" use=\"required\" /> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"subjobsType\"> \
		<xs:sequence> \
			<xs:element name=\"job\" type=\"jobType\" maxOccurs=\"unbounded\" /> \
		</xs:sequence> \
	</xs:complexType> \
	 \
	 \
	<xs:complexType name=\"scriptType\" mixed=\"true\"> \
		<xs:choice minOccurs=\"0\" maxOccurs=\"unbounded\"> \
			<xs:element name=\"value\" type=\"valueType\" /> \
		</xs:choice>       \
	</xs:complexType> \
	 \
	 \
	<xs:simpleType name=\"StrNonEmpty\"> \
		<xs:restriction base=\"xs:string\"> \
			<xs:minLength value=\"1\"/> \
		</xs:restriction> \
	</xs:simpleType> \
	 \
	 \
</xs:schema> \
";
