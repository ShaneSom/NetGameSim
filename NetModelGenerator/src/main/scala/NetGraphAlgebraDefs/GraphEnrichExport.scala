package NetGraphAlgebraDefs

import NetGraphAlgebraDefs.{Action, NetGraph, NetModelAlgebra, NodeObject}
import com.typesafe.config.ConfigFactory
import io.circe.syntax.*
import io.circe.generic.auto.*
import scala.jdk.CollectionConverters.*
import java.nio.charset.StandardCharsets
import java.nio.file.{Files, Paths}

object GraphEnrichExport:
  
  

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

  def enrichExport(inputFileName: String, outputFilePath: String): Either[String, ExportedNetGraph] =
  NetGraph.load(inputFileName) match
    case None =>
      Left(s"Failed to load graph from $inputFileName")

    case Some(netGraph) =>
      try
        val exported = fromNetGraph(netGraph)
        exported.validate()
        writeJson(exported, outputFilePath)
        Right(exported)
      catch
        case e: Exception =>
          Left(s"Export failed: ${e.getMessage}")

  def fromNetGraph(netGraph: NetGraph): ExportedNetGraph =
    val config = ConfigFactory.load()
    val directed =
      config.getConfig("NGSimulator").getConfig("Graph").getString("directionality") == "directed"

    val seed =
      if config.getConfig("NGSimulator").hasPath("seed") then
        config.getConfig("NGSimulator").getLong("seed")
      else 0L

    val nodes: Vector[Int] =
      netGraph.sm.nodes().asScala.toVector.map(_.id).sorted

    val edges: Vector[ExportedEdge] =
      netGraph.sm.edges().asScala.toVector.map { endpoint =>
        val srcNode = endpoint.nodeU()
        val dstNode = endpoint.nodeV()
        val action = netGraph.sm.edgeValue(srcNode, dstNode).get()

        ExportedEdge(
          src = srcNode.id,
          dst = dstNode.id,
          weight = convertWeight(action)
        )
      }

    ExportedNetGraph(
      seed = seed,
      directed = directed,
      initialNodeId = netGraph.initState.id,
      nodeCount = nodes.size,
      nodes = nodes,
      edges = dedupeEdges(edges, directed)
    )

  private def convertWeight(action: Action): Int =
    // Action.cost is a Double in [0,1], so map it to a positive integer weight.
    // This gives a stable range for MPI testing.
    math.max(1, math.ceil(action.cost * 20.0).toInt)

  private def dedupeEdges(
                           edges: Vector[ExportedEdge],
                           directed: Boolean
                         ): Vector[ExportedEdge] =
    if directed then
      edges.groupBy(e => (e.src, e.dst)).values.map(_.head).toVector.sortBy(e => (e.src, e.dst))
    else
      edges
        .groupBy { e =>
          val a = math.min(e.src, e.dst)
          val b = math.max(e.src, e.dst)
          (a, b)
        }
        .values
        .map(_.head)
        .toVector
        .sortBy(e => (math.min(e.src, e.dst), math.max(e.src, e.dst)))

  private def writeJson(graph: ExportedNetGraph, outputFilePath: String): Unit =
    val path = Paths.get(outputFilePath)
    val parent = path.getParent
    if parent != null then Files.createDirectories(parent)

    val json = graph.asJson.spaces2
    Files.write(
      path,
      json.getBytes(StandardCharsets.UTF_8)
    )

  @main def runGraphExporter(inputFileName: String, outputFilePath: String): Unit =
    enrichExport(inputFileName, outputFilePath) match
    case Left(err) =>
      println(s"[GraphExporter] ERROR: $err")
      System.exit(1)

    case Right(graph) =>
      println(s"[GraphExporter] Exported graph successfully.")
      println(s"[GraphExporter] Nodes: ${graph.nodeCount}")
      println(s"[GraphExporter] Edges: ${graph.edges.size}")
      println(s"[GraphExporter] Init node: ${graph.initialNodeId}")
      println(s"[GraphExporter] Output: $outputFilePath")