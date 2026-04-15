package NetGraphPartitioner


final case class ExportedEdge(
                               src: Int,
                               dst: Int,
                               weight: Int
                             )

final case class ExportedNetGraph(
                                   seed: Long,
                                   directed: Boolean,
                                   initialNodeId: Int,
                                   nodeCount: Int,
                                   nodes: Vector[Int],
                                   edges: Vector[ExportedEdge]
                                 ) {
  def validate(): Unit = {
    require(nodes.distinct.size == nodes.size, "Duplicate node IDs found")
    require(nodeCount == nodes.size, "nodeCount does not match nodes.size")
    require(nodes.contains(initialNodeId), "initialNodeId is not in nodes")
    require(edges.forall(_.weight > 0), "All edge weights must be positive")

    val nodeSet = nodes.toSet
    require(
      edges.forall(e => nodeSet.contains(e.src) && nodeSet.contains(e.dst)),
      "Edge contains invalid node ID"
    )
  }
}