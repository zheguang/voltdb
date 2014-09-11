/* This file is part of VoltDB.
 * Copyright (C) 2008-2014 VoltDB Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with VoltDB.  If not, see <http://www.gnu.org/licenses/>.
 */

package org.hsqldb_voltpatches;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.TreeMap;

/**
 * Used to fake generate XML without actually generating the text and parsing it.
 * A performance optimization, and something of a simplicity win
 *
 */
public class VoltXMLElement {

    public String name;
    public final Map<String, String> attributes = new TreeMap<String, String>();
    public final List<VoltXMLElement> children = new ArrayList<VoltXMLElement>();

    public VoltXMLElement(String name) {
        this.name = name;
    }

    public VoltXMLElement withValue(String key, String value) {
        attributes.put(key, value);
        return this;
    }

    public String getRealName() {
        return attributes.get("name");
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        append(sb, "");
        return sb.toString();
    }

    private void append(StringBuilder sb, String indent) {
        sb.append(indent).append("ELEMENT: ").append(name).append("\n");
        for (Entry<String, String> e : attributes.entrySet()) {
            sb.append(indent).append(" ").append(e.getKey());
            sb.append(" = ").append(e.getValue()).append("\n");
        }
        if ( ! children.isEmpty()) {
            sb.append(indent).append("[").append("\n");
            for (VoltXMLElement e : children) {
                e.append(sb, indent + "   ");
            }
        }
    }

    public VoltXMLElement duplicate() {
        VoltXMLElement retval = new VoltXMLElement(name);
        for (Entry<String, String> e : attributes.entrySet()) {
            retval.attributes.put(e.getKey(), e.getValue());
        }
        for (VoltXMLElement child : children) {
            retval.children.add(child.duplicate());
        }
        return retval;
    }

    public VoltXMLElement findChild(String name)
    {
        for (VoltXMLElement vxe : children) {
            if (name.equals(vxe.name)) {
                return vxe;
            }
        }
        return null;
    }

    /**
     * Get a string representation that is designed to be as short as possible
     * with as much certainty of uniqueness as possible.
     * A SHA-1 hash would suffice, but here's hoping just dumping to a string is
     * faster. Will measure later.
     */
    public String toMinString() {
        StringBuilder sb = new StringBuilder();
        toMinString(sb);
        return sb.toString();
    }

    protected StringBuilder toMinString(StringBuilder sb) {
        sb.append("\tE").append(name).append('\t');
        for (Entry<String, String> e : attributes.entrySet()) {
            sb.append('\t').append(e.getKey());
            sb.append('\t').append(e.getValue());
        }
        sb.append("\t[");
        for (VoltXMLElement e : children) {
            e.toMinString(sb);
        }
        return sb;
    }

    static public class VoltXMLDiff
    {
        final String m_name;
        List<VoltXMLElement> m_addedNodes = new ArrayList<VoltXMLElement>();
        List<VoltXMLElement> m_removedNodes = new ArrayList<VoltXMLElement>();
        Map<String, VoltXMLDiff> m_changedNodes = new HashMap<String, VoltXMLDiff>();
        Map<String, String> m_addedAttributes = new HashMap<String, String>();
        Set<String> m_removedAttributes = new HashSet<String>();
        Map<String, String> m_changedAttributes = new HashMap<String, String>();

        public VoltXMLDiff(String name)
        {
            // May need to change this to the toMinString() result for the VoltXMLElement
            // it matches instead
            m_name = name;
        }

        public List<VoltXMLElement> getAddedNodes()
        {
            return m_addedNodes;
        }

        public List<VoltXMLElement> getRemovedNodes()
        {
            return m_removedNodes;
        }

        public Map<String, VoltXMLDiff> getChangedNodes()
        {
            return m_changedNodes;
        }

        public Map<String, String> getAddedAttributes()
        {
            return m_addedAttributes;
        }

        public Set<String> getRemovedAttributes()
        {
            return m_removedAttributes;
        }

        public Map<String, String> getChangedAttributes()
        {
            return m_changedAttributes;
        }

        public boolean isEmpty()
        {
            return (m_addedNodes.isEmpty() &&
                    m_removedNodes.isEmpty() &&
                    m_changedNodes.isEmpty() &&
                    m_addedAttributes.isEmpty() &&
                    m_removedAttributes.isEmpty() &&
                    m_changedAttributes.isEmpty());
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append("NAME: " + m_name + "\n");
            sb.append("ADDED: " + m_addedAttributes + "\n");
            sb.append("REMOVED: " + m_removedAttributes + "\n");
            sb.append("CHANGED: " + m_changedAttributes + "\n");
            sb.append("NEW CHILDREN:\n");
            for (VoltXMLElement add : m_addedNodes) {
                sb.append(add.toString());
            }
            sb.append("DEAD CHILDREN:\n");
            for (VoltXMLElement remove : m_removedNodes) {
                sb.append(remove.toString());
            }
            sb.append("CHANGED CHILDREN:\n");
            for (VoltXMLDiff change : m_changedNodes.values()) {
                sb.append(change.toString());
            }
            sb.append("\n\n");
            return sb.toString();
        }
    }

    static public VoltXMLDiff computeDiff(VoltXMLElement first, VoltXMLElement second)
    {
        // Top level call needs both names to match (I think this makes sense)
        if (!first.name.equals(second.name)) {
            // not sure this is best behavior, ponder as progress is made
            return null;
        }

        VoltXMLDiff result = new VoltXMLDiff(first.name);
        // first, check the attributes
        Set<String> firstKeys = first.attributes.keySet();
        Set<String> secondKeys = new HashSet<String>();
        secondKeys.addAll(second.attributes.keySet());
        // Do removed and changed attributes walking the first element's attributes
        for (String firstKey : firstKeys) {
            if (!secondKeys.contains(firstKey)) {
                result.m_removedAttributes.add(firstKey);
            }
            else if (!(second.attributes.get(firstKey).equals(first.attributes.get(firstKey)))) {
                result.m_changedAttributes.put(firstKey, second.attributes.get(firstKey));
            }
            // remove the firstKey from secondKeys to track things added
            secondKeys.remove(firstKey);
        }
        // everything in secondKeys should be something added
        for (String key : secondKeys) {
            result.m_addedAttributes.put(key, second.attributes.get(key));
        }

        // Now, need to check the children.  Each pair of children with the same names
        // need to be descended to look for changes
        // Probably more efficient ways to do this, but brute force it for now
        // Would be helpful if the underlying children objects were Maps rather than
        // Lists.
        Set<String> firstChildren = new HashSet<String>();
        for (VoltXMLElement child : first.children) {
            firstChildren.add(child.name);
        }
        Set<String> secondChildren = new HashSet<String>();
        for (VoltXMLElement child : second.children) {
            secondChildren.add(child.name);
        }
        Set<String> commonNames = new HashSet<String>();
        for (VoltXMLElement firstChild : first.children) {
            if (!secondChildren.contains(firstChild.name)) {
                result.m_removedNodes.add(firstChild);
            }
            else {
                commonNames.add(firstChild.name);
            }
        }
        for (VoltXMLElement secondChild : second.children) {
            if (!firstChildren.contains(secondChild.name)) {
                result.m_addedNodes.add(secondChild);
            }
            else {
                assert(commonNames.contains(secondChild.name));
            }
        }

        for (String name : commonNames) {
            VoltXMLDiff childDiff = computeDiff(first.findChild(name), second.findChild(name));
            if (!childDiff.isEmpty()) {
                result.m_changedNodes.put(name, childDiff);
            }
        }

        return result;
    }

    public boolean applyDiff(VoltXMLDiff diff)
    {
        // Can only apply a diff to the root at which it was generated
        assert(name.equals(diff.m_name));

        // Do the attribute changes
        attributes.putAll(diff.getAddedAttributes());
        for (String key : diff.getRemovedAttributes()) {
            attributes.remove(key);
        }
        for (Entry<String,String> e : diff.getChangedAttributes().entrySet()) {
            attributes.put(e.getKey(), e.getValue());
        }

        // Do the node removals and additions
        for (VoltXMLElement e : diff.getRemovedNodes()) {
            children.remove(findChild(e.name));
        }
        for (VoltXMLElement e : diff.getAddedNodes()) {
            children.add(e);
        }

        // To do the node changes, recursively apply the inner diffs to the children
        for (Entry<String, VoltXMLDiff> e : diff.getChangedNodes().entrySet()) {
            findChild(e.getKey()).applyDiff(e.getValue());
        }

        return true;
    }
}
