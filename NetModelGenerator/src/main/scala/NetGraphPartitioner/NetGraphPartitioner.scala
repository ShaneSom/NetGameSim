package NetGraphPartition

import io.circe.generic.auto.*
import io.circe.parser.decode
import io.circe.syntax.*
import java.nio.charset.StandardCharsets
import java.nio.file.{Files, Paths}

object GraphPartitioner:

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
                                   ):
    def validate(): Unit =
      require(nodes.distinct.size == nodes.size, "Duplicate node IDs found")
      require(nodeCount == nodes.size, "nodeCount does not match nodes.size")
      require(nodes.contains(initialNodeId), "initialNodeId is not in nodes")
      require(edges.forall(_.weight > 0), "All edge weights must be positive")

      val nodeSet = nodes.toSet
      require(
        edges.forall(e => nodeSet.contains(e.src) && nodeSet.contains(e.dst)),
        "Edge contains invalid node ID"
      )

  final case class BoundaryEdge(
                                 src: Int,
                                 dst: Int,
                                 srcOwner: Int,
                                 dstOwner: Int
                               )

  final case class PartitionedGraph(
                                     ranks: Int,
                                     initialNodeId: Int,
                                     ownerOf: Map[Int, Int],
                                     ownedNodesByRank: Map[Int, Vector[Int]],
                                     boundaryEdges: Vector[BoundaryEdge]
                                   ):
    def validate(): Unit =
      require(ranks > 0, "Number of ranks must be positive")
      require(ownerOf.nonEmpty, "No nodes found in partition")
      require(
        ownerOf.values.forall(r => r >= 0 && r < ranks),
        "Invalid rank assignment found"
      )
      require(
        ownerOf.contains(initialNodeId),
        "Initial node is not assigned to any rank"
      )

      val reconstructed =
        ownedNodesByRank.toVector
          .flatMap { case (rank, nodes) => nodes.map(n => n -> rank) }
          .toMap

      require(
        reconstructed == ownerOf,
        "ownedNodesByRank does not match ownerOf mapping"
      )

      val allAssigned = ownedNodesByRank.values.flatten.toVector
      require(
        allAssigned.distinct.size == allAssigned.size,
        "A node was assigned to more than one rank"
      )

      require(
        boundaryEdges.forall(be => be.srcOwner != be.dstOwner),
        "Boundary edge contains same-owner endpoints"
      )

  def partitionContiguous(graph: ExportedNetGraph, ranks: Int): PartitionedGraph =
    graph.validate()
    require(ranks > 0, "ranks must be positive")
    require(ranks <= graph.nodeCount, "ranks cannot exceed nodeCount for simple partitioner")

    val sortedNodes = graph.nodes.sorted
    val n = sortedNodes.size

    val ownerOf: Map[Int, Int] =
      sortedNodes.zipWithIndex.map { case (nodeId, idx) =>
        val rank = (idx * ranks) / n
        nodeId -> rank
      }.toMap

    val ownedNodesByRank: Map[Int, Vector[Int]] =
      (0 until ranks).map { rank =>
        rank -> sortedNodes.filter(nodeId => ownerOf(nodeId) == rank).toVector
      }.toMap

    val boundaryEdges: Vector[BoundaryEdge] =
      graph.edges.flatMap { e =>
        val srcOwner = ownerOf(e.src)
        val dstOwner = ownerOf(e.dst)
        if srcOwner != dstOwner then
          Some(BoundaryEdge(e.src, e.dst, srcOwner, dstOwner))
        else None
      }

    val partition = PartitionedGraph(
      ranks = ranks,
      initialNodeId = graph.initialNodeId,
      ownerOf = ownerOf,
      ownedNodesByRank = ownedNodesByRank,
      boundaryEdges = boundaryEdges
    )

    partition.validate()
    partition

  def loadExportedGraph(inputFilePath: String): Either[String, ExportedNetGraph] =
    try
      val json = Files.readString(Paths.get(inputFilePath), StandardCharsets.UTF_8)
      decode[ExportedNetGraph](json) match
        case Left(err)    => Left(s"Failed to parse graph JSON: ${err.getMessage}")
        case Right(graph) =>
          graph.validate()
          Right(graph)
    catch
      case e: Exception => Left(s"Failed to load graph JSON: ${e.getMessage}")

  def writePartition(partition: PartitionedGraph, outputFilePath: String): Either[String, Unit] =
    try
      val path = Paths.get(outputFilePath)
      val parent = path.getParent
      if parent != null then Files.createDirectories(parent)

      val json = partition.asJson.spaces2
      Files.write(path, json.getBytes(StandardCharsets.UTF_8))
      Right(())
    catch
      case e: Exception => Left(s"Failed to write partition JSON: ${e.getMessage}")

  def partitionFile(
                     inputGraphPath: String,
                     ranks: Int,
                     outputPartitionPath: String
                   ): Either[String, PartitionedGraph] =
    for
      graph <- loadExportedGraph(inputGraphPath)
      partition = partitionContiguous(graph, ranks)
      _ <- writePartition(partition, outputPartitionPath)
    yield partition

  @main def runGraphPartitioner(
                                 inputGraphPath: String,
                                 ranks: Int,
                                 outputPartitionPath: String
                               ): Unit =
    println(s"[GraphPartitioner] Loading exported graph: $inputGraphPath")
    println(s"[GraphPartitioner] Using ranks: $ranks")

    partitionFile(inputGraphPath, ranks, outputPartitionPath) match
      case Left(err) =>
        println(s"[GraphPartitioner] ERROR: $err")
        System.exit(1)

      case Right(partition) =>
        println("[GraphPartitioner] Partition successful!")
        println(s"Ranks: ${partition.ranks}")
        println(s"Initial node: ${partition.initialNodeId}")
        println(s"Assigned nodes: ${partition.ownerOf.size}")
        println(s"Boundary edges: ${partition.boundaryEdges.size}")
        partition.ownedNodesByRank.toVector.sortBy(_._1).foreach { case (rank, nodes) =>
          println(s"Rank $rank owns ${nodes.size} nodes: ${nodes.mkString(", ")}")
        }
        println(s"Output written to: $outputPartitionPath")