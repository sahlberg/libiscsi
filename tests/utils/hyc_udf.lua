function hyc_udf_fn(rec, ckpt_id)
	
	local binName = 'data_map'
	local val = bytes(0)
	local xrec = map()

	trace("HYC_UDF (%d) start----------------------", ckpt_id);
	if aerospike:exists(rec) then
	        local l = rec[binName]
        	if (l == nil) then
			return nil
        	else
			-- rec is a map, extract data corresponding
			-- to checkpoint id

			if l[ckpt_id] == nil then
				return nil
			end

			trace("HYC_UDF (%d) step2 ----------------------", ckpt_id);
			local ckpt_val = l[ckpt_id]
			bytes.append_bytes(val, ckpt_val['data'], bytes.size(ckpt_val['data']))
			--val = ckpt_val['data']
			trace("HYC_UDF val (%s) :::: ----------------------", val);
			xrec['data'] = ckpt_val['data']
			return xrec
			--return "hello world"
        	end
	else
		return nil;--end return empty array
	end--end else aerospike exists

	trace("HYC_UDF (%d) END--------------------", ckpt_id);
end
