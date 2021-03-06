#include <gtest/gtest.h>
#include <rai/node/testing.hpp>

TEST (conflicts, start_stop)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::send_block> (genesis.hash (), key1.pub, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0));
	ASSERT_EQ (rai::process_result::progress, node1.process (*send1).code);
	ASSERT_EQ (0, node1.active.roots.size ());
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	ASSERT_EQ (1, node1.active.roots.size ());
	auto root1 (send1->root ());
	auto existing1 (node1.active.roots.find (root1));
	ASSERT_NE (node1.active.roots.end (), existing1);
	auto votes1 (existing1->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (1, votes1->votes.rep_votes.size ());
}

TEST (conflicts, add_existing)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::send_block> (genesis.hash (), key1.pub, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0));
	ASSERT_EQ (rai::process_result::progress, node1.process (*send1).code);
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	rai::keypair key2;
	auto send2 (std::make_shared<rai::send_block> (genesis.hash (), key2.pub, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0));
	node1.active.start (send2);
	ASSERT_EQ (1, node1.active.roots.size ());
	auto vote1 (std::make_shared<rai::vote> (key2.pub, key2.prv, 0, send2));
	node1.active.vote (vote1);
	ASSERT_EQ (1, node1.active.roots.size ());
	auto votes1 (node1.active.roots.find (send2->root ())->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (2, votes1->votes.rep_votes.size ());
	ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (key2.pub));
}

TEST (conflicts, add_two)
{
	rai::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	rai::genesis genesis;
	rai::keypair key1;
	auto send1 (std::make_shared<rai::send_block> (genesis.hash (), key1.pub, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0));
	ASSERT_EQ (rai::process_result::progress, node1.process (*send1).code);
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	rai::keypair key2;
	auto send2 (std::make_shared<rai::send_block> (send1->hash (), key2.pub, 0, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0));
	ASSERT_EQ (rai::process_result::progress, node1.process (*send2).code);
	node1.active.start (send2);
	ASSERT_EQ (2, node1.active.roots.size ());
}

TEST (votes, contested)
{
	rai::genesis genesis;
	auto block1 (std::make_shared<rai::state_block> (rai::test_genesis_key.pub, genesis.hash (), rai::test_genesis_key.pub, rai::genesis_amount - rai::Gxrb_ratio, rai::test_genesis_key.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0));
	auto block2 (std::make_shared<rai::state_block> (rai::test_genesis_key.pub, genesis.hash (), rai::test_genesis_key.pub, rai::genesis_amount - 2 * rai::Gxrb_ratio, rai::test_genesis_key.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, 0));
	ASSERT_FALSE (*block1 == *block2);
	rai::votes votes (block1);
	ASSERT_TRUE (votes.uncontested ());
	votes.rep_votes[rai::test_genesis_key.pub] = block2;
	ASSERT_FALSE (votes.uncontested ());
}

TEST (conflicts, rollback_source)
{
	rai::system system (24000, 2);
	auto & node1 (*system.nodes[0]);
	auto & node2 (*system.nodes[1]);
	rai::genesis genesis;
	rai::keypair key1;
	system.wallet (0)->insert_adhoc (rai::test_genesis_key.prv);
	auto send1 (std::make_shared<rai::state_block> (rai::test_genesis_key.pub, genesis.hash (), rai::test_genesis_key.pub, rai::genesis_amount - rai::Gxrb_ratio, key1.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, system.work.generate (genesis.hash ())));
	ASSERT_EQ (rai::process_result::progress, node2.process (*send1).code);
	ASSERT_FALSE (node2.active.start (send1));
	ASSERT_EQ (rai::process_result::progress, node1.process (*send1).code);
	ASSERT_FALSE (node1.active.start (send1));
	auto send2 (std::make_shared<rai::state_block> (rai::test_genesis_key.pub, send1->hash (), rai::test_genesis_key.pub, rai::genesis_amount - 2 * rai::Gxrb_ratio, key1.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, system.work.generate (send1->hash ())));
	ASSERT_EQ (rai::process_result::progress, node2.process (*send2).code);
	ASSERT_FALSE (node2.active.start (send2));
	auto send3 (std::make_shared<rai::state_block> (key1.pub, 0, key1.pub, rai::Gxrb_ratio, send1->hash (), key1.prv, key1.pub, system.work.generate (key1.pub)));
	ASSERT_EQ (rai::process_result::progress, node2.process (*send3).code);
	ASSERT_FALSE (node2.active.start (send3));
	auto send4 (std::make_shared<rai::state_block> (key1.pub, send3->hash (), key1.pub, 2 * rai::Gxrb_ratio, send2->hash (), key1.prv, key1.pub, system.work.generate (send3->hash ())));
	ASSERT_EQ (rai::process_result::progress, node2.process (*send4).code);
	ASSERT_FALSE (node2.active.start (send4));
	auto send5 (std::make_shared<rai::state_block> (rai::test_genesis_key.pub, send1->hash (), rai::test_genesis_key.pub, rai::genesis_amount - 3 * rai::Gxrb_ratio, key1.pub, rai::test_genesis_key.prv, rai::test_genesis_key.pub, system.work.generate (send1->hash ())));
	ASSERT_EQ (rai::process_result::progress, node1.process (*send5).code);
	ASSERT_FALSE (node1.active.start (send5));
	auto iterations (0);
	while (!node2.active.active (*send4))
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	while (node2.active.active (*send4))
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
}
