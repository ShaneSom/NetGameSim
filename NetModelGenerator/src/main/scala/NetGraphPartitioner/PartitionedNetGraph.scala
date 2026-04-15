package NetGraphPartition

final case class PartitionedGraph(
                                   ranks: Int,
                                   initialNodeId: Int,
                                   ownerOf: Map[Int, Int],
                                   ownedNodesByRank: Map[Int, Vector[Int]]
                                 ) {

  def validate(): Unit = {
    require(ranks > 0, "Number of ranks must be positive")

    val allNodes = ownerOf.keySet

    // Every node must be assigned exactly once
    require(allNodes.nonEmpty, "No nodes found in partition")

    // All ranks must be valid
    require(
      ownerOf.values.forall(r => r >= 0 && r < ranks),
      "Invalid rank assignment found"
    )

    // ownedNodesByRank must match ownerOf
    val reconstructed =
      ownedNodesByRank.flatMap { case (rank, nodes) =>
        nodes.map(n => n -> rank)
      }.toMap

    require(
      reconstructed == ownerOf,
      "ownedNodesByRank does not match ownerOf mapping"
    )

    // initial node must exist
    require(
      ownerOf.contains(initialNodeId),
      "Initial node is not assigned to any rank"
    )
  }
}