using System;
using System.Collections.Generic;
using System.Linq;

namespace NumCIL.Bohrium
{
	public static class DependencyBuilder
	{
		/// <summary>
		/// The different types of nodes
		///</summary>
		private enum NodeType
		{
			/// <summary>
			/// The node represents an instruction
			/// </summary>
			Instruction,
			/// <summary>
			/// The node represents a collection
			/// </summary>
			Collection,
			/// <summary>
			/// The node represents a read fence
			/// </summary>
			ReadFence,
			/// <summary>
			/// The node represents a write fence
			/// </summary>
			WriteFence
		}
		
		/// <summary>
		/// The node used in the graph
		/// </summary>
		private class Node
		{
			/// <summary>
			/// The left child.
			/// </summary>
			public Node LeftChild;
			/// <summary>
			/// The right child.
			/// </summary>
			public Node RightChild;
			/// <summary>
			/// The node type.
			/// </summary>
			public NodeType Type;
			/// <summary>
			/// The instruction.
			/// </summary>
			public PInvoke.bh_instruction Instruction;
			/// <summary>
			/// The fence target.
			/// </summary>
			public Node FenceTarget;
			/// <summary>
			/// An optional node tag.
			/// </summary>
			public string Tag;
			
			/// <summary>
			/// Parent 1.
			/// </summary>
			public Node P1;
			/// <summary>
			/// Parent 2
			/// </summary>
			public Node P2;
			
			/// <summary>
			/// Creates a normal instruction type node, and optionally attaches it to a parent.
			/// </summary>
			/// <param name="instruction">The instruction to represent</param>
			/// <param name="parent">The optional node parent</param>
			public Node(PInvoke.bh_instruction instruction)
			{
				this.Instruction = instruction;
				this.Type = NodeType.Instruction;
			}
			
			/// <summary>
			/// Attaches a specific type of node as a leaf underneath the parent.
			/// </summary>
			/// <param name="type">The type of node to create</param>
			/// <param name="parent">The parent that the node is attached to</param>
			public Node(NodeType type)
			{
				this.Type = type;
			}
							
			/// <summary>
			/// Appends a child to this node and updates the parent of the child
			/// <param name="newchild">The child to append</param>
			/// </summary>		
			public void AppendChild(Node newchild)
			{
				if (this.LeftChild == null)
				{
					this.LeftChild = newchild;
					newchild.AppendParent(this);
				}
				else if (this.RightChild == null)
				{
					this.RightChild = newchild;
					newchild.AppendParent(this);
				}
				else
				{
					var cn = new Node(NodeType.Collection);
					cn.LeftChild = this.LeftChild;
					cn.RightChild = newchild;
					this.LeftChild = cn;

					if (cn.LeftChild.P1 == this)
						cn.LeftChild.P1 = cn;
					else if (cn.LeftChild.P2 == this)
						cn.LeftChild.P2 = cn;
					else
						throw new Exception("Bad graph");					
						
					newchild.AppendParent(cn);
					cn.P1 = this;
				}
			}
			
			/// <summary>
			/// Sets the parent of this node
			/// <param name="newparent">The new parent</param>
			/// </summary>		
			public void AppendParent(Node newparent)
			{
				if (this.P1 == newparent || this.P2 == newparent || newparent == null)
					return;
				else if (this.P1 == null)
					this.P1 = newparent;
				else if (this.P2 == null)
					this.P2 = newparent;
				else
				{
					var cn = new Node(NodeType.Collection);
					cn.P1 = this.P1;
					cn.P2 = this.P2;
					
					if (this.P1.LeftChild == this)
						cn.P1.LeftChild = cn;
					else if (this.P1.RightChild == this)
						cn.P1.RightChild = cn;
					
					if (this.P2.LeftChild == this)
						cn.P2.LeftChild = cn;
					else if (this.P2.RightChild == this)
						cn.P2.RightChild = cn;
					
					
					this.P1 = cn;
					this.P2 = newparent;
					cn.LeftChild = this;
				}
			}
			
			/// <summary>
			/// Returns a <see cref="System.String"/> that represents the current <see cref="NumCIL.Bohrium.DependencyBuilder+Node"/>.
			/// </summary>
			/// <returns>A <see cref="System.String"/> that represents the current <see cref="NumCIL.Bohrium.DependencyBuilder+Node"/>.</returns>
			public override string ToString()
			{
				if (this.Tag != null)
					return this.Tag;
				else if (this.Type == NodeType.Instruction)
					return this.Instruction.opcode.ToString();
				else
					return this.Type.ToString();
			}

			/// <summary>
			/// Injects the node before this node.
			/// </summary>
			/// <param name="cur">Current</param>
			public void InjectBefore(Node cur)
			{
				this.LeftChild = cur;
				if (cur.P1 != null)
				{
					if (cur.P1.LeftChild == cur)
						cur.P1.LeftChild = this;
					else if (cur.P1.RightChild == cur)
						cur.P1.RightChild = this;
					else
						throw new Exception("Bad graph");
					
					this.P1 = cur.P1;
				}
				
				if (cur.P2 != null)
				{
					if (cur.P2.LeftChild == cur)
						cur.P2.LeftChild = this;
					else if (cur.P2.RightChild == cur)
						cur.P2.RightChild = this;
					else
						throw new Exception("Bad graph");
					
					this.P2 = cur.P2;
				}
				
				cur.P1 = this;
				cur.P2 = null;
			}
		}
		
#if DEBUG
		/// <summary>
		/// Numbering helper to output all graphs generated in a run.
		/// </summary>
		private static long FILENAMECOUNT = 0;
#endif
	
		/// <summary>
		/// Entry function that reorganizes the instructions.
		/// </summary>
		/// <returns>The reorganized instruction set</returns>
		/// <param name="instructions">The instructions to organize</param>
		public static PInvoke.bh_instruction[] ReorganizeInstructions(PInvoke.bh_instruction[] instructions)
		{
#if DEBUG		
			var filename = FILENAMECOUNT++;
			PrintGraph(instructions, "from-list-" + filename + ".dot");

			var bareroots = BuildGraph(instructions, false, false).ToList();
			PrintGraph(bareroots, "bare-tree-" + filename + ".dot");

			var discroots = BuildGraph(instructions, false, true).ToList();
			PrintGraph(discroots, "bare-discard-tree-" + filename + ".dot");
			
			var fencedroots = BuildGraph(instructions, true, false).ToList();
			PrintGraph(fencedroots, "fenced-tree-" + filename + ".dot");

			var fenceddiscroots = BuildGraph(instructions, true, true).ToList();
			PrintGraph(fenceddiscroots, "fenced-discard-tree-" + filename + ".dot");
			
			var serialized = Serialize(fenceddiscroots);
			PrintGraph(serialized, "serialized-list-" + filename + ".dot");
						
			/*foreach (var n in serialized)
				Console.WriteLine("Actual instr: {0}", n);*/
				
			if (serialized.Count() != instructions.Length)
			{
				Console.WriteLine("Serialized instruction count: {0} but should be {1}", serialized.Count(), instructions.Length);
				
				var tmp = new Dictionary<PInvoke.bh_instruction, string>();
				foreach (var n in instructions)
				{
					//Console.WriteLine("Actual instr: {0}", n);
					tmp.Add(n, null);
				}
			
				foreach (var n in serialized)
					if (!tmp.ContainsKey(n))
					{
						Console.WriteLine("Extra instr: {0}", n);
					}
					else
					{
						tmp.Remove(n);
					}
				
				foreach (var n in tmp.Keys)
					Console.WriteLine("Missing instr: {0}", n);
			
			}
			
			return serialized.ToArray();
#else
			var roots = BuildGraph(instructions, true, true);
			if (roots == null)
				return instructions;
			else
				return Serialize(roots).ToArray();
#endif
		}
		
		/// <summary>
		/// Gets the array ID.
		/// </summary>
		/// <returns>The array ID.</returns>
		/// <param name="array">The array to get the ID for</param>
		private static long GetArrayID(PInvoke.bh_array_ptr array)
		{
			if (array == PInvoke.bh_array_ptr.Null)
				return 0;
			if (array.BaseArray != PInvoke.bh_array_ptr.Null)
				array = array.BaseArray;
			return array.PtrValue;
		}
		
		/// <summary>
		/// Serialize the graph consisting of the specified roots.
		/// </summary>
		/// <param name="roots">The graph roots</param>
		private static IEnumerable<PInvoke.bh_instruction> Serialize(IEnumerable<Node> roots)
		{
			// Keep track of already scheduled nodes
			var scheduled = new Dictionary<Node, Node>();
			// Keep track of items that have unsatisfied dependencies
			var blocked = new Queue<Node>(roots);
			//Pick up the result
			var result = new List<PInvoke.bh_instruction>();
			
			while (blocked.Count > 0)
			{
				var n = blocked.Dequeue();
				if (!scheduled.ContainsKey(n))
				{
					// Check if dependencies are met
					if ((n.P1 == null || scheduled.ContainsKey(n.P1)) && (n.P2 == null || scheduled.ContainsKey(n.P2)))
					{
						if (n.Type == NodeType.Instruction)
							result.Add(n.Instruction);
						scheduled.Add(n, n);
						
						//Examine child nodes
						if (n.LeftChild != null)
							blocked.Enqueue(n.LeftChild);
						if (n.RightChild != null && n.RightChild != n.LeftChild)
							blocked.Enqueue(n.RightChild);
					}
					else
					{
						// Re-insert at bottom of work queue
						blocked.Enqueue(n);
					}
				}
			}
			
			return result;
		}
		
		/// <summary>
		/// Builds the graph from the list of instructions.
		/// </summary>
		/// <returns>The graph roots</returns>
		/// <param name="instructions">The list of instructions</param>
		/// <param name="injectFences">If set to <c>true</c> inject fence nodes.</param>
		/// <param name="attachDiscards">If set to <c>true</c> attaches discards nodes.</param>
		private static IEnumerable<Node> BuildGraph(IEnumerable<PInvoke.bh_instruction> instructions, bool injectFences, bool attachDiscards)
		{
			var res = new List<Node>();
			var map = new Dictionary<long, Node>();
			var roots = new List<Node>();
			var readMap = new Dictionary<long, Node>();
			var writeMap = new Dictionary<long, Node>();
			var discards = new List<Node>();
			var instrCount = 0L;
			
			
			foreach (var i in instructions)
			{
				var selfId = GetArrayID(i.operand0);
				var leftId = GetArrayID(i.operand1);
				var rightId = GetArrayID(i.operand2);
				if (selfId == 0)
				{
					var uf = i.UserfuncIdNOutNIn;
					if (uf == null || uf.Item2 != 1 || (uf.Item3 != 0 && uf.Item3 != 1 && uf.Item3 != 2))
					{
						Console.WriteLine("Bailing because the userfunc is weird :(");
						return null;
					}
					
					var arrays = i.UserfuncArrays;
					selfId = GetArrayID(arrays[0]);
					if (arrays.Length > 1)
						leftId = GetArrayID(arrays[1]);
					if (arrays.Length > 2)
						rightId = GetArrayID(arrays[2]);
				}
					
				//Console.WriteLine("Node {0}, has {1} {2} {3}", i.opcode, selfId, GetArrayID(i.operand1), GetArrayID(i.operand2));
				
				var selfNode = new Node(i);
				selfNode.Tag = "I" + (instrCount++) + " - " + i.opcode.ToString();
				
				if (i.opcode == bh_opcode.BH_DISCARD || i.opcode == bh_opcode.BH_SYNC)
				{
					if (attachDiscards)
						discards.Add(selfNode);
				}
				else
				{				
					Node oldTarget;
					map.TryGetValue(selfId, out oldTarget);
					if (oldTarget != null)
						oldTarget.AppendChild(selfNode);	
					
					map[selfId] = selfNode;
					
					Node leftDep;
					Node rightDep;
					map.TryGetValue(leftId, out leftDep);
					map.TryGetValue(rightId, out rightDep);
	
					if (leftDep != null)
					{
						leftDep.AppendChild(selfNode);
					}
					if (rightDep != null && rightDep != leftDep)
					{
						rightDep.AppendChild(selfNode);
					}
					
					if (leftDep == null && rightDep == null && oldTarget == null)
						roots.Add(selfNode);
	
					res.Add(selfNode);
					if (injectFences)
						InjectFence(selfNode, readMap, writeMap);
				}
			}
			
			Func<Node, Node> injectDiscard = (d) =>
			{
				var exploration = new Queue<Node>();
				Node cur;
				map.TryGetValue(GetArrayID(d.Instruction.operand0), out cur);
				
				map[GetArrayID(d.Instruction.operand0)] = d;
				
				if (cur == null)
				{
					return d;
				}
				else
				{
					if (cur.LeftChild != null)
					{
						cur = cur.LeftChild;
						if (cur.RightChild != null)
							exploration.Enqueue(cur.RightChild);
					}
					else if (cur.RightChild != null)
						cur = cur.RightChild;
					
					while (cur != null)
					{
						if (cur.Type == NodeType.Instruction)
						{
							cur.AppendChild(d);
							if (exploration.Count > 0)
								cur = exploration.Dequeue();
							else
								cur = null;
						}
						else
						{
							if (cur.LeftChild != null)
							{
								cur = cur.LeftChild;
								if (cur.RightChild != null)
									exploration.Enqueue(cur.RightChild);
							}
							else if (cur.RightChild != null)
								cur = cur.RightChild;
							else if (exploration.Count > 0)
								cur = exploration.Dequeue();
							else
								cur = null;
						}
					}
					
					return null;
				}
			};
			
			//Attach all views
			foreach (var d in discards.Where(x => x.Instruction.opcode == bh_opcode.BH_DISCARD && x.Instruction.operand0.BaseArray != PInvoke.bh_array_ptr.Null))
				if (injectDiscard(d) != null)
					roots.Add(d);

			//Attach all bases
			foreach (var d in discards.Where(x => x.Instruction.opcode == bh_opcode.BH_DISCARD && x.Instruction.operand0.BaseArray == PInvoke.bh_array_ptr.Null))
				if (injectDiscard(d) != null)
					roots.Add(d);
			
			foreach (var d in discards.Where(x => x.Instruction.opcode == bh_opcode.BH_SYNC))
				if (injectDiscard(d) != null)
					roots.Add(d);
			
			return roots;
		}
		
		/// <summary>
		/// Injects fence instructions into the graph
		/// </summary>
		/// <returns>The injected fence</returns>
		/// <param name="cur">The current element in the list</param>
		/// <param name="readMap">The lookup table for read operands.</param>
		/// <param name="writeMap">The lookup table for written operands.</param>
		private static Node InjectFence(Node cur, Dictionary<long, Node> readMap, Dictionary<long, Node> writeMap)
		{
			if (cur.Type != NodeType.Instruction || cur.Instruction.opcode == bh_opcode.BH_DISCARD)
				return cur;
		
			Node p;
			var writeBase = GetArrayID(cur.Instruction.operand0);
			var readBase1 = GetArrayID(cur.Instruction.operand1);
			var readBase2 = GetArrayID(cur.Instruction.operand2);
			if (writeBase == 0)
			{
				var uf = cur.Instruction.UserfuncIdNOutNIn;
				if (uf == null || uf.Item2 != 1 || (uf.Item3 != 0 && uf.Item3 != 1 && uf.Item3 != 2))
				{
					Console.WriteLine("Bailing because the userfunc is too weird :(");
					return null;
				}
				
				var arrays = cur.Instruction.UserfuncArrays;
				writeBase = GetArrayID(arrays[0]);
				if (arrays.Length > 1)
					readBase1 = GetArrayID(arrays[1]);
				if (arrays.Length > 2)
					readBase2 = GetArrayID(arrays[2]);
			}
			
						
			if (readMap.TryGetValue(writeBase, out p))
			{
				//We need a write-fence
				var n = new Node(NodeType.WriteFence);
				n.FenceTarget = p;
				n.InjectBefore(cur);
				
				//cur = n;
				readMap.Remove (writeBase);
			}
			
			if (readBase1 != 0)
			{
				if (writeMap.TryGetValue(readBase1, out p))
				{
					//We need a read-fence
					var n = new Node(NodeType.ReadFence);
					n.FenceTarget = p;
					n.InjectBefore(cur);
					
					//cur = n;
					writeMap.Remove(readBase1);
				}
			}
			
			if (readBase2 != 0)
			{
				if (writeMap.TryGetValue(readBase2, out p))
				{
					//We need a read-fence
					var n = new Node(NodeType.ReadFence);
					n.FenceTarget = p;
					n.InjectBefore(cur);
					
					//cur = n;
					writeMap.Remove(readBase2);
				}
				
			}
			
			if (writeBase != 0)
				writeMap[writeBase] = cur;
			if (readBase1 != 0 && writeBase != readBase1)
				readMap[readBase1] = cur;
			if (readBase2 != 0 && writeBase != readBase2 && readBase1 != readBase2)
				readMap[readBase2] = cur;
				
			return cur;
		}
		
		/// <summary>
		/// Prints the graph in DOT format
		/// </summary>
		/// <param name="roots">List of graph roots</param>
		/// <param name="file">The file to write to</param>
		private static void PrintGraph(IEnumerable<Node> roots, string file)
		{
			var duplicateRemover = new Dictionary<string, string>();
			using (var fs = new System.IO.StreamWriter(file, false, System.Text.Encoding.UTF8))
			{
				fs.WriteLine("digraph {");
				
				var queue = new Queue<Node>();
				foreach(var n in roots)
					queue.Enqueue(n);
				
				var nameTable = new Dictionary<Node, string>();
				var lastInstName = 0L;
				var lastReadFenceName = 0L;
				var lastWriteFenceName = 0L;
				var lastColName = 0L;
				
				Func<Node, string> getName = (Node n) => {
					string s;
					if (!nameTable.TryGetValue(n, out s))
					{
						if (n.Type == NodeType.Instruction)
							s = "I" + (lastInstName++).ToString();
						else if (n.Type == NodeType.Collection)
							s = "C" + (lastColName++).ToString();
						else if (n.Type == NodeType.ReadFence)
							s = "R" + (lastReadFenceName++).ToString();
						else if (n.Type == NodeType.WriteFence)
							s = "W" + (lastWriteFenceName++).ToString();
						else
							throw new Exception(string.Format("Unexpected nodetype: {0}", n.Type));
						nameTable[n] = s;
					}
					return s;
				};
				
				while(queue.Count > 0)
				{
					var node = queue.Dequeue();
					if (node.LeftChild != null)
						queue.Enqueue(node.LeftChild);
					if (node.RightChild != null)
						queue.Enqueue(node.RightChild);
					
					if (node.Type == NodeType.Instruction)
					{
						var color = roots.Contains(node) ? "#CBffff" : "#CBD5E8";
						var label = string.IsNullOrEmpty(node.Tag) ? string.Format("{0} - {1}", getName(node), node.Instruction.opcode) : node.Tag;
						var shape = node.Instruction.opcode == bh_opcode.BH_DISCARD ? "box" : "box";
						var style = node.Instruction.opcode == bh_opcode.BH_DISCARD ? "dashed,rounded" : "filled,rounded";
						fs.WriteLine(@"{0} [shape={1}, style={2}, fillcolor=""{3}"", label=""{4}""];", getName(node), shape, style, color, label);
						
					}
					else if (node.Type == NodeType.Collection)
					{
						fs.WriteLine(@"{0} [shape=box, style=filled, fillcolor=""#ffffE8"", label=""{0} - {1}""];", getName(node), "COLLECTION");
					}
					else if (node.Type == NodeType.ReadFence)
					{
						fs.WriteLine(@"{0} [shape=triangle, style=filled, fillcolor=""#CBD5E8"", label=""{0} - {1}""];", getName(node), "R");
						var line = string.Format("{0} -> {1} [color=red, style=dotted]; ", getName(node), getName(node.FenceTarget));
						if (!duplicateRemover.ContainsKey(line))
							fs.WriteLine(line);
						duplicateRemover[line] = line;
					}
					else if (node.Type == NodeType.WriteFence)
					{
						fs.WriteLine(@"{0} [shape=triangle, style=filled, fillcolor=""#CBD5E8"", label=""{0} - {1}""];", getName(node), "W");
						var line = string.Format("{0} -> {1} [color=red, style=dotted]; ", getName(node), getName(node.FenceTarget));
						if (!duplicateRemover.ContainsKey(line))
							fs.WriteLine(line);
						duplicateRemover[line] = line;
					}
					
					if (node.LeftChild != null)
					{
						var line = string.Format("{0} -> {1};", getName(node), getName(node.LeftChild));
						if (!duplicateRemover.ContainsKey(line))
							fs.WriteLine(line);
						duplicateRemover[line] = line;
					}
					if (node.RightChild != null)
					{
						var line = string.Format("{0} -> {1};", getName(node), getName(node.RightChild));
						if (!duplicateRemover.ContainsKey(line))
							fs.WriteLine(line);
						duplicateRemover[line] = line;
					}
				}
				
				fs.WriteLine("}");
			}
		}
		
		/// <summary>
		/// Prints the graph in DOT format
		/// </summary>
		/// <param name="insts">List of instructions</param>
		/// <param name="file">The file to write to</param>
		private static void PrintGraph(IEnumerable<PInvoke.bh_instruction> insts, string file)
		{
			var baseNameDict = new Dictionary<long, string>();
			var instrNameDict = new Dictionary<PInvoke.bh_instruction, string>();
			
			var lastBaseName = 0;
			var lastInstrName = 0;
			var constName = 0;
			
			using (var fs = new System.IO.StreamWriter(file, false, System.Text.Encoding.UTF8))
			{
				fs.WriteLine("digraph {");
				
				foreach(var n in insts)				
				{
					if (n.opcode != bh_opcode.BH_USERFUNC)
					{
						string instName;
						if (!instrNameDict.TryGetValue(n, out instName))
						{
							instName = "I_" + (lastInstrName++).ToString();
							instrNameDict[n] = instName;
						}
						
						var baseID = GetArrayID(n.operand0);
						var leftID = GetArrayID(n.operand1);
						var rightID = GetArrayID(n.operand2);
												
						string parentBaseName;
						string leftBaseName;
						string rightBaseName;
						
						if (!baseNameDict.TryGetValue(baseID, out parentBaseName))
						{
							parentBaseName = "B_" + (lastBaseName++).ToString();
							baseNameDict[baseID] = parentBaseName;
						}
						
						if (!baseNameDict.TryGetValue(leftID, out leftBaseName))
						{
							leftBaseName = "B_" + (lastBaseName++).ToString();
							baseNameDict[leftID] = leftBaseName;
						}
						
						if (!baseNameDict.TryGetValue(rightID, out rightBaseName))
						{
							rightBaseName = "B_" + (lastBaseName++).ToString();
							baseNameDict[rightID] = rightBaseName;
						}
						
						var nops = PInvoke.bh_operands(n.opcode);
						
												
						if (nops == 3)
						{
							if (leftID == 0)
							{
								var constid = "const_" + (constName++).ToString();
								fs.WriteLine(@"{0} [shape=pentagon, style=filled, fillcolor=""#ff0000"", label=""{1}""];", constid, n.constant.ToString());
								fs.WriteLine("{0} -> {1};", constid, instName);								
							}
							else
							{
								fs.WriteLine(@"{0} [shape=ellipse, style=filled, fillcolor=""#0000ff"", label=""{0} -{1}""];", leftBaseName, n.operand1.BaseArray.PtrValue);
								fs.WriteLine("{0} -> {1};", leftBaseName, instName);
							}
								
							if (rightID == 0)
							{
								var constid = "const_" + (constName++).ToString();
								fs.WriteLine(@"{0} [shape=pentagon, style=filled, fillcolor=""#ff0000"", label=""{1}""];", constid, n.constant.ToString());
								fs.WriteLine("{0} -> {1};", constid, instName);
							}
							else
							{
								fs.WriteLine(@"{0} [shape=ellipse, style=filled, fillcolor=""#0000ff"", label=""{0} -{1}""];", rightBaseName, n.operand2.BaseArray.PtrValue);
								fs.WriteLine("{0} -> {1};", rightBaseName, instName);
							}
							
						}
						else if (nops == 2)
						{
							if (leftID == 0)
							{
								var constid = "const_" + (constName++).ToString();
								fs.WriteLine(@"{0} [shape=pentagon, style=filled, fillcolor=""#ff0000"", label=""{1}""];", constid, n.constant.ToString());
								fs.WriteLine("{0} -> {1};", constid, instName);								
							}
							else
							{
								fs.WriteLine(@"{0} [shape=ellipse, style=filled, fillcolor=""#0000ff"", label=""{0} -{1}""];", leftBaseName, n.operand1.BaseArray.PtrValue);
								fs.WriteLine("{0} -> {1};", leftBaseName, instName);
							}
						}
						
						fs.WriteLine(@"{0} [shape=box, style=filled, fillcolor=""#CBD5E8"", label=""{0} - {1}""];", instName, n.opcode);
						fs.WriteLine(@"{0} [shape=ellipse, style=filled, fillcolor=""#0000ff"", label=""{0} - {1}""];", parentBaseName, n.operand0.BaseArray.PtrValue);
						
						fs.WriteLine("{0} -> {1};", instName, parentBaseName);
					}
				}
				
				fs.WriteLine("}");
			}
		}
	}
}

