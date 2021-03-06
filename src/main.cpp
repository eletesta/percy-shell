#include <percy/percy.hpp>
#include <alice/alice.hpp>

vector<kitty::dynamic_truth_table> functions;

namespace percy
{
    /***************************************************************************
        A generic logic network class used to store the results of exact
        synthesis.
    ***************************************************************************/
    class unbound_logic_network
    {
    private:
        int nr_in;
        std::vector<std::vector<int>> nodes;
        std::vector<kitty::dynamic_truth_table> operators;
        std::vector<int> outputs;
    
    public:
        unbound_logic_network() {}

        template<int FI>
        void
        copy_chain(const chain<FI>& c)
        {
            nr_in = c.get_nr_inputs();

            nodes.clear();
            operators.clear();
            
            c.foreach_vertex([this, &c] (auto v, int i) {
                std::vector<int> fanin;
                c.foreach_fanin(v, [&fanin] (auto fid, int j) {
                    fanin.push_back(fid);
                });
                this->nodes.push_back(fanin);
                
                const auto& static_op = c.get_operator(i);
                dynamic_truth_table op(fanin.size());

                for (int k = 0; k < static_op.num_bits(); k++) {
                    if (get_bit(static_op, k))
                        set_bit(op, k);
                }

                this->operators.push_back(op);
            });
        }

        int get_nr_in() const { return nr_in; }
        int get_nr_out() const { return outputs.size(); }
        int get_nr_nodes() const { return nodes.size(); }
        const auto& get_nodes() const { return nodes; }
        const auto& get_node(int i) const { return nodes[i]; }
        const auto& get_operator(int i) const { return operators[i]; }
    };
}

namespace alice
{
    using percy::synth_spec;
    using percy::chain;
    using kitty::dynamic_truth_table;
    using percy::unbound_logic_network;

    ALICE_ADD_STORE(percy::synth_spec<kitty::dynamic_truth_table>, 
            "spec", "s", "specification", "specification");
    
    ALICE_ADD_STORE(percy::unbound_logic_network, 
            "ntk", "n", "network", "networks");

    ALICE_DESCRIBE_STORE(unbound_logic_network, ntk)
    {
        return fmt::format("({}, {}, {})", 
                            ntk.get_nr_in(), 
                            ntk.get_nr_nodes(), 
                            ntk.get_nr_out());
    }

    ALICE_PRINT_STORE(unbound_logic_network, os, ntk)
    {
        const auto nr_in = ntk.get_nr_in();
        char node_name = 'A' + nr_in;
        for (int i = 0; i < ntk.get_nr_nodes(); i++) {
            os << node_name << " = " << kitty::to_binary(ntk.get_operator(i));
            auto fanins = ntk.get_node(i);
            for (auto fanin : fanins) {
                char fanin_name = '\0';
                if (fanin < nr_in) {
                    fanin_name = 'a' + fanin;
                } else {
                    fanin_name = 'A' + nr_in + (fanin - nr_in);
                }
                os << " " << fanin_name;
            }
            os << "\n";
            node_name++;
        }
    }

    /***************************************************************************
        Loads a truth table and adds it to the corresponding store.
    ***************************************************************************/
    class load_spec_command : public command
    {
        public:

            load_spec_command(const environment::ptr& env) : 
                command( env, "Create new specification" )
            {
                add_option(
                    "truth_table,--tt", truth_table, 
                    "truth table in hex format");
                add_flag( "--binary,-b", "read truth table as binary string" );
            }

        protected:

            void 
            execute() override
            {
                auto num_vars = 0u;
                dynamic_truth_table function;

                if (is_set( "binary" )) {     
                    num_vars = ::log(truth_table.size()) / ::log(2.0);
                    kitty::dynamic_truth_table f(num_vars);
                    kitty::create_from_binary_string(f, truth_table);
                    function = f;
                } else {
                    num_vars = ::log(truth_table.size() * 4) / ::log(2.0);
                    kitty::dynamic_truth_table f(num_vars);
                    kitty::create_from_hex_string(f, truth_table);
                    function = f;
                }
                functions.push_back(function);
                const auto fptr = &functions[functions.size() - 1];

                synth_spec<dynamic_truth_table> spec(num_vars, 1);
                spec.functions[0] = fptr;

                this->store<synth_spec<dynamic_truth_table>>().extend() = spec;
            }

        private:
            std::string truth_table;
    };

    ALICE_DESCRIBE_STORE(percy::synth_spec<kitty::dynamic_truth_table>, spec)
    {
        return fmt::format("({}, {}, {})", 
                            spec.get_nr_in(), 
                            spec.get_nr_out(), 
                            kitty::to_hex(*spec.functions[0]));
    }

    ALICE_PRINT_STORE(percy::synth_spec<kitty::dynamic_truth_table>, os, spec)
    {
        os << "SPECIFICATION\n";
        os << "Nr. inputs = " << spec.get_nr_in() << "\n";
        os << "Nr. outputs = " << spec.get_nr_out() << "\n";
        for (int i = 0; i < spec.get_nr_out(); i++) {
            os << "f_" << i + 1 << " = ";
            os << kitty::to_hex(*spec.functions[i]) << " (hex) -- ";
            os << kitty::to_binary(*spec.functions[i]) << " (bin)\n";
        }
    }

    ALICE_ADD_COMMAND(load_spec, "Specification");

    class synthesize_command : public command
    {
        public:
            synthesize_command(const environment::ptr& env) : 
                command( env, "Synthesize network from specification" ),
                fanin_size(0)
            {
                add_option(
                    "fanin,-k", fanin_size, 
                    "fanin size of network operators");
            }

            void 
            execute() override
            {
                if (this->store<synth_spec<dynamic_truth_table>>().size()==0) {
                    this->env->err() << "Error: specification not found\n";
                    return;
                }
                if (fanin_size <= 1 || fanin_size > 5) {
                    this->env->err() << "Error: fanin size " << fanin_size
                        << " is not supported\n";
                    return;
                }
                auto spec = 
                    this->store<synth_spec<dynamic_truth_table>>().current();


                percy::synth_result result;
                if (fanin_size == 2) {
                    chain<2> c;
                    auto synth = percy::new_std_synth<2>();
                    result = synth->synthesize<dynamic_truth_table>(spec, c);
                    unbound_logic_network ntk;
                    ntk.copy_chain(c);
                    this->store<unbound_logic_network>().extend() = ntk;
                } else if (fanin_size == 3) {
                    chain<3> c;
                    auto synth = percy::new_std_synth<3>();
                    result = synth->synthesize<dynamic_truth_table>(spec, c);
                    unbound_logic_network ntk;
                    ntk.copy_chain(c);
                    this->store<unbound_logic_network>().extend() = ntk;
                } else if (fanin_size == 4) {
                    chain<4> c;
                    auto synth = percy::new_std_synth<4>();
                    result = synth->synthesize<dynamic_truth_table>(spec, c);
                    unbound_logic_network ntk;
                    ntk.copy_chain(c);
                    this->store<unbound_logic_network>().extend() = ntk;
                } else {
                    chain<5> c;
                    auto synth = percy::new_std_synth<5>();
                    result = synth->synthesize<dynamic_truth_table>(spec, c);
                    unbound_logic_network ntk;
                    ntk.copy_chain(c);
                    this->store<unbound_logic_network>().extend() = ntk;
                }

                switch (result) {
                    case percy::synth_result::success:
                        this->env->out() << "SUCCESS\n";
                        break;
                    case percy::synth_result::failure:
                        this->env->out() << "FAILURE\n";
                        break;
                    case percy::synth_result::timeout:
                        this->env->out() << "TIMEOUT\n";
                        break;
                }
                this->env->out() << "Time elapsed: " << spec.synth_time 
                    << "\u03BCs\n";
            }
        private:
            int fanin_size;
    };
    
    ALICE_ADD_COMMAND(synthesize, "Synthesis");

}

ALICE_MAIN(percy)

