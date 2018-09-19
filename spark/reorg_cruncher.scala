def reorg(datadir :String) 
{
  val t0 = System.nanoTime()

  // nothing here (yet)

  val t1 = System.nanoTime()
  println("reorg time: " + (t1 - t0)/1000000 + "ms")
}

def cruncher(datadir :String, a1 :Int, a2 :Int, a3 :Int, a4 :Int, lo :Int, hi :Int) :org.apache.spark.sql.DataFrame =
{
   val t0 = System.nanoTime()

  // load the three tables
  val person   = spark.read.format("csv").option("header", "true").option("delimiter", "|").option("inferschema", "true").
                       load(datadir + "/person.*csv.*")
  val interest = spark.read.format("csv").option("header", "true").option("delimiter", "|").option("inferschema", "true").
                       load(datadir + "/interest.*csv.*")
  val knows    = spark.read.format("csv").option("header", "true").option("delimiter", "|").option("inferschema", "true").
                       load(datadir + "/knows.*csv.*")

  // select the relevant (personId, interest) tuples, and add a boolean column "nofan" (true iff this is not a a1 tuple)
  val focus    = interest.filter($"interest" isin (a1, a2, a3, a4)).
                          withColumn("nofan", $"interest".notEqual(a1))

  // compute person score (#relevant interests): join with focus, groupby & aggregate. Note: nofan=true iff person does not like a1
  val scores   = person.join(focus, "personId").
                        groupBy("personId", "locatedIn", "birthday").
                        agg(count("personId") as "score", min("nofan") as "nofan")

  // filter (personId, score, locatedIn) tuples with score>1, being nofan, and having the right birthdate
  val cands    = scores.filter($"score" > 0 && $"nofan").
                        withColumn("bday", month($"birthday")*100 + dayofmonth($"birthday")).
                        filter($"bday" >= lo && $"bday" <= hi)

  // create (personId, ploc, friendId, score) pairs by joining with knows (and renaming locatedIn into ploc)
  val pairs    = cands.select($"personId", $"locatedIn".alias("ploc"), $"score").
                       join(knows, "personId")

  // re-use the scores dataframe to create a (friendId, floc) dataframe of persons who are a fan (not nofan)
  val fanlocs  = scores.filter(!$"nofan").select($"personId".alias("friendId"), $"locatedIn".alias("floc"))

  // join the pairs to get a (personId, ploc, friendId, floc, score), and then filter on same location, and remove ploc and floc columns
  val results  = pairs.join(fanlocs, "friendId").
                       filter($"ploc"===$"floc").
                       select($"personId".alias("p"), $"friendId".alias("f"), $"score")

  // do the bidirectionality check by joining towards knows, and keeping only the (p, f, score) pairs where also f knows p
  val bidir    = results.join(knows.select($"personId".alias("f"), $"friendId"), "f").filter($"p"===$"friendId")

  // keep only the (p, f, score) columns and sort the result
  val ret      = bidir.select($"p", $"f", $"score").orderBy(desc("score"), asc("p"), asc("f"))

  ret.show(1000) // force execution now, and display results to stdout

  val t1 = System.nanoTime()
  println("cruncher time: " + (t1 - t0)/1000000 + "ms")

  return ret
}
